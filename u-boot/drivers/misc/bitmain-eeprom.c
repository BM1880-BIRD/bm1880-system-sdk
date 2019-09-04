#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <misc.h>
#include <bitmain-mcu.h>

#define MCU_ID_REG		(0)

static void __maybe_unused show_mac(void *mac)
{
	int i;

	for (i = 0; i < MCU_MAC_SIZE; ++i) {
		printf("%02x", ((uint8_t *)mac)[i]);
		if (i != MCU_MAC_SIZE - 1)
			printf(":");
	}
	printf("\n");
}

static int bmeeprom_reset(struct udevice *i2c)
{
	struct i2c_msg msg;
	u8 offset[2];
	struct dm_i2c_chip *chip = dev_get_parent_platdata(i2c);

	memset(&msg, 0, sizeof(msg));
	memset(offset, 0, sizeof(offset));

	/* 16bits offset, big endian */
	msg.addr = chip->chip_addr;
	msg.buf = offset;
	msg.len = sizeof(offset);
	msg.flags = 0;

	return dm_i2c_xfer(i2c, &msg, 1);
}

static inline void eeprom_convert_cpu_offset(uint8_t *buf, uint16_t offset)
{
	__be16 i2c_off = __cpu_to_be16(offset);

	buf[0] = ((uint8_t *)(&i2c_off))[0];
	buf[1] = ((uint8_t *)(&i2c_off))[1];
}

static int __maybe_unused workaround_bmeeprom_read(
	struct udevice *i2c, int offset, void *buf, int size)
{
	struct i2c_msg msg[2];
	u8 offset_buf[2];
	int err, i;
	struct dm_i2c_chip *chip = dev_get_parent_platdata(i2c);

	dev_dbg(i2c, "read %s from 0x%04x size 0x%02x\n",
		i2c->name, offset, size);

	memset(msg, 0, sizeof(msg));
	/* 16bits offset, big endian */
	msg[0].addr = chip->chip_addr;
	msg[0].len = sizeof(offset_buf);
	msg[0].flags = 0;

	msg[1].addr = chip->chip_addr;
	msg[1].len = 1;
	msg[1].flags = I2C_M_RD;

	/* data to be read */
	for (i = 0; i < size; ++i) {
		eeprom_convert_cpu_offset(offset_buf, offset + i);
		msg[0].buf = offset_buf;

		msg[1].buf = ((uint8_t *)buf) + i;

		err = dm_i2c_xfer(i2c, msg, ARRAY_SIZE(msg));
		if (err) {
			dev_err(i2c, "i2c transfer error\n");
			return err;
		}
		dev_dbg(i2c, "offset %04x value %02x\n",
			offset + i, *(uint8_t *)(msg[1].buf));
	}
	return size;
}

static int __maybe_unused bmeeprom_read(
	struct udevice *i2c, int offset, void *buf, int size)
{
	struct i2c_msg msg[2];
	u8 offset_buf[2];
	int err;
	struct dm_i2c_chip *chip = dev_get_parent_platdata(i2c);

	dev_dbg(i2c, "read %s from 0x%04x size 0x%02x\n",
		i2c->name, offset, size);
	offset_buf[0] = (offset >> 8) & 0xff;
	offset_buf[1] = offset & 0xff;

	memset(msg, 0, sizeof(msg));
	/* 16bits offset, big endian */
	msg[0].addr = chip->chip_addr;
	msg[0].buf = offset_buf;
	msg[0].len = sizeof(offset_buf);
	msg[0].flags = 0;

	/* data to be read */
	msg[1].addr = chip->chip_addr;
	msg[1].buf = buf;
	msg[1].len = size;
	msg[1].flags = I2C_M_RD;

	err = dm_i2c_xfer(i2c, msg, ARRAY_SIZE(msg));
	if (err) {
		dev_err(i2c, "i2c transfer error\n");
		return err;
	}
	return size;
}

static int bmeeprom_probe(struct udevice *i2c)
{
	i2c_set_chip_flags(i2c, DM_I2C_CHIP_RD_ADDRESS |
			   DM_I2C_CHIP_WR_ADDRESS);

	return bmeeprom_reset(i2c);
}

static const struct misc_ops bmeeprom_ops = {
	.read = workaround_bmeeprom_read,
};

static const struct udevice_id bmeeprom_ids[] = {
	{ .compatible = "bitmain,bm16xx-eeprom" },
	{},
};

U_BOOT_DRIVER(eeprom) = {
	.name	= "bitmain-eeprom",
	.id	= UCLASS_MISC,
	.probe	= bmeeprom_probe,
	.of_match = bmeeprom_ids,
	.ops	= &bmeeprom_ops,
};

