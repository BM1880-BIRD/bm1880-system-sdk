#include <stdlib.h>
#include <common.h>
#include <config.h>
#include <command.h>
#include <part.h>
#include <vsprintf.h>

#define DEBUG
#ifdef DEBUG
#define pr_debug(fmt, ...) \
	printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

struct bcb_cmd_var {
	char *cmd;
	char *var;
};

struct bcb_cmd_var bcb_cmd_var[] = {
	[0] = {.cmd = "boot-recovery", .var = "boot-recovery"},
	[1] = {.cmd = "burn-firmware", .var = "burn_firmware"},
};

static int do_mmcbcb(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret, p;
	struct blk_desc *desc;
	char *var = NULL;
	char *bcb_ptr = NULL;
	disk_partition_t info;

	if (argc != 4)
		return CMD_RET_USAGE;

	pr_debug("%s %s %s\n", argv[1], argv[2], argv[3]);
	ret = blk_get_device_by_str(argv[1], argv[2], &desc);
	if (ret < 0)
		return CMD_RET_FAILURE;

	var = argv[3];

	for (p = 1; p < 128; p++) {
		int r = part_get_info(desc, p, &info);

		if (r == 0) {
			pr_debug("info.name:%s info.start:%ld info.size:%ld info.blksz:%ld\n", info.name, info.start, info.size, info.blksz);
			if (!strcmp((char *)info.name, "MISC")) {
				break;
			}
		} else {
			continue;
		}

	}

	if (p >= 128)
		return CMD_RET_FAILURE;

	bcb_ptr = memalign(info.blksz, info.blksz * 2);
	if (blk_dread(desc, (lbaint_t)info.start, 2, bcb_ptr) != 2) {
		printf("blk_dread err, count != 2\n");
		return CMD_RET_FAILURE;
	}

	for (int i = 0; i < sizeof(bcb_cmd_var)/sizeof(struct bcb_cmd_var); ++i) {
		if (!strncmp(bcb_ptr, bcb_cmd_var[i].cmd, strlen(bcb_cmd_var[i].cmd))) {
			setenv(var, (char *)bcb_cmd_var[i].var);
			return CMD_RET_SUCCESS;
		}
	}

	setenv(var, "None");

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	mmcbcb,	4,	1,	do_mmcbcb,
	"bootloader control block command",
	"bm_bcb <interface> <dev> <varname>\n"
);

