#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <misc.h>
#include <bitmain-mcu.h>

#define MCU_ID_REG		(0)

struct mcu_feature {
	u8 id;
	char *proj;
	char *soc;
	char *chip;

	int cmd;
};

struct eeprom_cell {
	int node;
	const char *name;
	uint32_t off;
	uint32_t size;
};

#define MCU_CELL_MAX	(64)
#define MCU_NAME_MAX	(128)

struct mcu_ctx {
	struct udevice *dev;
	const struct mcu_feature *feature;
	unsigned int eeprom_cells_count;
	struct eeprom_cell eeprom_cells[MCU_CELL_MAX];
};

static const struct mcu_feature mcu_list[] = {
	{
		0x00, "EVB", "BM1684", "STM32",
		.cmd = 0x14,
	},
	{
		0x01, "SA5", "BM1684", "STM32",
		.cmd = 0x14,
	},
};

static void mcu_show(struct udevice *i2c, const struct mcu_feature * const f)
{
	dev_dbg(i2c, "%d %s %s %s\n", f->id, f->proj, f->soc, f->chip);
}

static const struct mcu_feature *mcu_get_feature(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mcu_list); ++i)
		if (mcu_list[i].id == id)
			return mcu_list + i;

	return NULL;
}

static void show_mac(void *mac)
{
	int i;

	for (i = 0; i < MCU_MAC_SIZE; ++i) {
		printf("%02x", ((uint8_t *)mac)[i]);
		if (i != MCU_MAC_SIZE - 1)
			printf(":");
	}
	printf("\n");
}

static int do_mcu(struct cmd_tbl_s *cmdtp, int flag,
		  int argc, char * const argv[])
{
	int err, i;
	uint8_t id, cmd, sn[MCU_SN_MAX + 1], mac[MCU_MAC_MAX][MCU_MAC_SIZE];
	struct udevice *mcu;
	const struct mcu_feature *feature;
	static const char * const cmd_name[] = {
		[MCU_CMD_NOP] = "No special command",
		[MCU_CMD_ABORTBOOT] = "Abort auto boot",
		[MCU_CMD_RECOVERY] = "Enter recovery mode",
	};

	if (uclass_get_device_by_name(UCLASS_MISC, "bm16xxmcu", &mcu)) {
		error("no bitmain-mcu found\n");
		return CMD_RET_FAILURE;
	}

	err = misc_call(mcu, MCU_CALL_ID, NULL, 0, &id, sizeof(id));
	if (err < 0) {
		error("Failed to get project ID\n");
		return CMD_RET_FAILURE;
	}
	printf("ID\t\t%d\n", id);
	feature = mcu_get_feature(id);
	if (!feature) {
		error("Unknown project\n");
		return CMD_RET_FAILURE;
	}
	printf("Board\t\t%s\n", feature->proj);
	printf("SoC\t\t%s\n", feature->soc);
	printf("MCU\t\t%s\n", feature->chip);

	memset(sn, 0, sizeof(sn));
	err = misc_call(mcu, MCU_CALL_SN, NULL, 0, sn, sizeof(sn));
	if (err < 0) {
		error("Failed to get SN\n");
		return CMD_RET_FAILURE;
	}

	printf("SN\t\t%s\n", sn);

	err = misc_call(mcu, MCU_CALL_MAC, NULL, 0, mac, sizeof(mac));
	if (err < 0) {
		error("Failed to get MAC address\n");
		return CMD_RET_FAILURE;
	}
	for (i = 0; i < err / MCU_MAC_SIZE; ++i) {
		printf("MAC%d\t\t", i);
		show_mac(mac[i]);
	}

	err = misc_call(mcu, MCU_CALL_CMD,
			NULL, 0, &cmd, sizeof(cmd));
	if (err < 0) {
		error("Failed to get command status\n");
		return CMD_RET_FAILURE;
	}

	if (cmd >= ARRAY_SIZE(cmd_name)) {
		error("Unknown command %u\n", cmd);
		return CMD_RET_FAILURE;
	}

	if (cmd != MCU_CMD_NOP)
		printf("%s\n", cmd_name[cmd]);

	return CMD_RET_SUCCESS;
}

static int mcu_i2c_read_byte(struct udevice *i2c, int reg)
{
	int err;
	uint8_t buf;

	err = dm_i2c_read(i2c, reg, &buf, 1);
	dev_dbg(i2c, "%s reg %02x value %02x err %d\n",
		__func__, reg, buf, err);
	return err < 0 ? err : buf;
}

static int __maybe_unused mcu_i2c_read_block(struct udevice *i2c, int reg,
					     int len, void *data)
{
	int err, i;

	err = dm_i2c_read(i2c, reg, data, len);
	for (i = 0; i < len; ++i)
		dev_dbg(i2c, "%s reg %02x value %02x err %d\n",
			__func__, reg + i, ((u8 *)data)[i], err);

	return err;
}

static int mcu_call_id(struct udevice *i2c, int msgid,
		       void *tx_msg, int tx_size,
		       void *rx_msg, int rx_size)
{
	struct mcu_ctx *ctx;

	if (rx_size <= 0)
		return -ENOMEM;

	ctx = dev_get_platdata(i2c);
	*(uint8_t *)rx_msg = ctx->feature->id;
	dev_dbg(i2c, "mcu id %d\n", ctx->feature->id);
	return 1;
}

static struct eeprom_cell *find_cell(struct mcu_ctx *ctx, const char *name)
{
	int i;

	for (i = 0; i < ctx->eeprom_cells_count; ++i) {
		if (strncmp(ctx->eeprom_cells[i].name, name, MCU_NAME_MAX) == 0)
			return ctx->eeprom_cells + i;
	}
	return NULL;
}

static int mcu_get_cell(struct mcu_ctx *ctx, struct eeprom_cell *cell,
			void *buf, int len)
{
	int err;
	struct udevice *eeprom;
	int dev_node;

	dev_node = fdt_parent_offset(gd->fdt_blob, cell->node);
	if (dev_node < 0) {
		dev_err(ctx->dev, "can not get eeprom device node offset %d\n",
			dev_node);
		return -ENODEV;
	}

	err = uclass_get_device_by_of_offset(UCLASS_MISC, dev_node, &eeprom);
	if (err) {
		dev_err(ctx->dev, "can not get eeprom device %d\n", err);
		return err;
	}

	if (cell->size > len) {
		dev_err(ctx->dev, "buffer too short for cell %s\n", cell->name);
		return -ENOMEM;
	}

	return misc_read(eeprom, cell->off, buf, cell->size);
}

/*
 * WARNING: not zero terminated
 */
static int mcu_call_sn(struct udevice *i2c, int msgid,
		       void *tx_msg, int tx_size,
		       void *rx_msg, int rx_size)
{
	struct mcu_ctx *ctx;
	struct eeprom_cell *cell;

	if (rx_size < MCU_SN_MAX)
		return -ENOMEM;

	ctx = dev_get_platdata(i2c);
	cell = find_cell(ctx, "sn");
	if (!cell) {
		dev_err(i2c, "sn cell not found\n");
		return -ENODEV;
	}
	return mcu_get_cell(ctx, cell, rx_msg, rx_size);
}

static int mcu_call_cmd(struct udevice *i2c, int msgid,
			void *tx_msg, int tx_size,
			void *rx_msg, int rx_size)
{
	struct mcu_ctx *ctx;
	int err;

	if (rx_size <= 0)
		return -ENOMEM;

	ctx = dev_get_platdata(i2c);
	err = mcu_i2c_read_byte(i2c, ctx->feature->cmd);
	if (err < 0)
		*(uint8_t *)rx_msg = 0;
	else
		*(uint8_t *)rx_msg = err;

	return 1;
}

static int mcu_call_mac(struct udevice *i2c, int msgid,
			void *tx_msg, int tx_size,
			void *rx_msg, int rx_size)
{
	struct mcu_ctx *ctx;
	struct eeprom_cell *cell;
	uint8_t buf[MCU_MAC_SIZE * MCU_MAC_MAX];
	char name[MCU_NAME_MAX];
	int err, i, n;

	ctx = dev_get_platdata(i2c);
	for (i = 0, n = 0; i < MCU_MAC_MAX; ++i) {
		snprintf(name, sizeof(name), "mac%d", i);
		cell = find_cell(ctx, name);
		if (!cell)
			continue;
		err = mcu_get_cell(ctx, cell, buf + n, MCU_MAC_SIZE);
		n += MCU_MAC_SIZE;
		if (err < 0) {
			dev_err(i2c, "failed to get %s address\n", name);
			return err;
		}
	}

	if (rx_size < n)
		return -ENOMEM;

	memcpy(rx_msg, buf, n);
	return n;
}

typedef int (*call_func)(struct udevice *, int, void *, int, void *, int);

static const call_func call_list[] = {
	[MCU_CALL_ID] = mcu_call_id,
	[MCU_CALL_MAC] = mcu_call_mac,
	[MCU_CALL_CMD] = mcu_call_cmd,
	[MCU_CALL_SN] = mcu_call_sn,
};

static int mcu_call(struct udevice *i2c, int msgid,
		    void *tx_msg, int tx_size,
		    void *rx_msg, int rx_size)
{
	if (msgid < 0 || msgid >= ARRAY_SIZE(call_list))
		return -1;
	return call_list[msgid](i2c, msgid, tx_msg, tx_size, rx_msg, rx_size);
}

int strncnt(const char *dst[], const char *str, int len)
{
	int i, count;

	dst[0] = str;
	for (i = 0, count = 0; i < len; ++i) {
		if (str[i] == 0)
			dst[++count] = str + i + 1;
	}
	return count;
}

/*
 * return cell number on success
 * nagtive number on failed
 */
static int bind_cells(struct mcu_ctx *ctx)
{
	int err, i;

	int nvmem_cells_size = dev_read_size(ctx->dev, "nvmem-cells");
	int nvmem_cells_count;
	uint32_t nvmem_cells_value[MCU_CELL_MAX];

	int nvmem_cell_names_size;
	int nvmem_cell_names_count;
	const char *nvmem_cell_names_value[MCU_CELL_MAX];
	const char *tmp;

	int reglen;
	const uint32_t *regval;

	struct eeprom_cell *eeprom_cell = ctx->eeprom_cells;

	ctx->eeprom_cells_count = 0;
	if (nvmem_cells_size <= 0) {
		dev_dbg(ctx->dev, "no nvmem-cells property\n");
		return 0;
	}

	nvmem_cells_count = nvmem_cells_size / sizeof(uint32_t);
	dev_dbg(ctx->dev, "nvmem-cells property count %d\n", nvmem_cells_count);

	if (nvmem_cells_count > MCU_CELL_MAX) {
		dev_err(ctx->dev, "too many nvmem-cells");
		return -ENOMEM;
	}

	err = dev_read_u32_array(ctx->dev, "nvmem-cells",
				 nvmem_cells_value, nvmem_cells_count);
	if (err) {
		dev_err(ctx->dev, "failed to get nvmem-cells value\n");
		return err;
	}

	for (i = 0; i < nvmem_cells_count; ++i)
		dev_dbg(ctx->dev, "nvmem-cells[%d] %u\n",
			i, nvmem_cells_value[i]);

	nvmem_cell_names_size =
		dev_read_size(ctx->dev, "nvmem-cell-names");

	if (nvmem_cell_names_size <= 0) {
		dev_err(ctx->dev, "no nvmem-cell-names property\n");
		return -EINVAL;
	}

	dev_dbg(ctx->dev, "nvmem-cell-names property size %d\n",
		nvmem_cell_names_size);

	tmp = dev_read_string(ctx->dev, "nvmem-cell-names");
	if (!tmp) {
		dev_err(ctx->dev, "failed to get nvmem-cell-names property\n");
		return -EINVAL;
	}

	nvmem_cell_names_count =
		strncnt(nvmem_cell_names_value, tmp, nvmem_cell_names_size);

	dev_dbg(ctx->dev, "nvmem-cell-names count %d\n", nvmem_cell_names_size);
	dev_dbg(ctx->dev, "nvmem-cell-names value\n");
	for (i = 0; i < nvmem_cell_names_count; ++i)
		dev_dbg(ctx->dev, "\t- %s\n", nvmem_cell_names_value[i]);

	if (nvmem_cells_count != nvmem_cell_names_count) {
		dev_err(ctx->dev, "nvmem-cells and nvmem-cell-names must contain the same number of elements\n");
		return -EINVAL;
	}

	for (i = 0; i < nvmem_cells_count; ++i, ++eeprom_cell) {
		eeprom_cell->node = fdt_node_offset_by_phandle(gd->fdt_blob,
							nvmem_cells_value[i]);
		eeprom_cell->name = nvmem_cell_names_value[i];

		regval = fdt_getprop(gd->fdt_blob, eeprom_cell->node,
				     "reg", &reglen);
		if (reglen != 2 * sizeof(uint32_t)) {
			dev_err(ctx->dev, "property reg with invalid size");
			return -EINVAL;
		}

		eeprom_cell->off = __be32_to_cpu(regval[0]);
		eeprom_cell->size = __be32_to_cpu(regval[1]);

		dev_dbg(ctx->dev, "cell %s offset 0x%04x size 0x%04x\n",
			eeprom_cell->name, eeprom_cell->off, eeprom_cell->size);
	}
	ctx->eeprom_cells_count = nvmem_cells_count;
	return nvmem_cells_count;
}

static int mcu_probe(struct udevice *i2c)
{
	int id;
	struct mcu_ctx *ctx;

	i2c_set_chip_flags(i2c, DM_I2C_CHIP_RD_ADDRESS |
			   DM_I2C_CHIP_WR_ADDRESS);

	id = mcu_i2c_read_byte(i2c, MCU_ID_REG);
	if (id < 0)
		return id;

	ctx = dev_get_platdata(i2c);
	ctx->dev = i2c;
	ctx->feature = mcu_get_feature(id);
	if (!ctx->feature) {
		dev_err(i2c, "undefined mcu id %d\n", id);
		return -ENODEV;
	}
	mcu_show(i2c, ctx->feature);

	/* bind eeprom cells */
	if (bind_cells(ctx) < 0)
		dev_err(i2c, "can not bind cells\n");

	return 0;
}

static const struct misc_ops mcu_ops = {
	.call = mcu_call,
};

static const struct udevice_id mcu_ids[] = {
	{ .compatible = "bitmain,bm16xx-mcu" },
	{},
};

U_BOOT_DRIVER(mcu) = {
	.name	= "bitmain-mcu",
	.id	= UCLASS_MISC,
	.probe	= mcu_probe,
	.of_match = mcu_ids,
	.ops	= &mcu_ops,
	.platdata_auto_alloc_size = sizeof(struct mcu_ctx),
};

U_BOOT_CMD(
	board, 1, 1, do_mcu,
	"Get board information\n",
	"mcu\n"
);
