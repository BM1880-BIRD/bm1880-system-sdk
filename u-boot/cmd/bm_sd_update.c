#include <stdlib.h>
#include <common.h>
#include <config.h>
#include <command.h>
#include <fs.h>
#include <part.h>
#include <vsprintf.h>
#include <u-boot/md5.h>
#include <image-sparse.h>
#include <bm_emmc_burn.h>
#include <div64.h>
#include <linux/math64.h>
#include <fdt.h>

#ifdef DEBUG
#define pr_debug(fmt, ...) \
	printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

#define		BIT_WRITE_FIP_BIN	BIT(0)
#define		BIT_WRITE_ROM_PATCH	BIT(1)
#define		BIT_WRITE_BLD		BIT(2)

#define		COMPARE_STRING_LEN	3

static int do_bm_update_emmc(uint32_t component, void *addr)
{
	struct blk_desc *boot1_desc, *boot2_desc;
	int ret = 0, err = 0;
	uint32_t size = 0;

	printf("do_bm_update_emmc com 0x%x\n", component);

	component & BIT_WRITE_ROM_PATCH ? printf("write rom patch to emmc\n") : printf("do nothing\n");
	component & BIT_WRITE_BLD ? printf("write bld to emmc\n") : printf("do nothing\n");

	// 0.1 : 0 means emmc, 1 means 1st boot area
	ret = blk_get_device_by_str("mmc", "0.1", &boot1_desc);
	if (ret < 0) {
		printf("get mmc 0.1 device failed\n");
		return CMD_RET_FAILURE;
	}

	if (component & BIT_WRITE_FIP_BIN) {
		printf("write fip.bin to emmc\n");
		err = blk_dwrite(boot1_desc, 0, (512 * 1024) / 512, addr); // write 512k fip.bin
	}

	// 0.2 : 0 means emmc, 2 means 2nd boot area
	ret = blk_get_device_by_str("mmc", "0.2", &boot2_desc);
	if (ret < 0) {
		printf("get mmc 0.2 device failed\n");
		return CMD_RET_FAILURE;
	}

	if (component & BIT_WRITE_ROM_PATCH) {
		printf("write rom patch to emmc\n");
		err = blk_dwrite(boot2_desc, 0, size, addr);
	}

	if (component & BIT_WRITE_BLD) {
		printf("write bld to emmc\n");
		err = blk_dwrite(boot2_desc, 0, size, addr);
	}

	(void)err;

	return 0;
}

static int do_bm_sd_update(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *addr;
//	uint32_t size = 0;
//	uint32_t img_blk_cnt = 0;
//	char dst[16];
	uint32_t component = 0;

	if (argc != 4) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	addr = (char *)simple_strtol(argv[3], NULL, 16);

	if (!addr) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	printf("add %p\n", addr);

	if (!strncmp(argv[2], "fip", COMPARE_STRING_LEN)) {
		printf("fip.bin\n");
		component |= BIT_WRITE_FIP_BIN;

	} else if (!strncmp(argv[2], "patch", COMPARE_STRING_LEN)) {
		printf("patch\n");
		component |= BIT_WRITE_ROM_PATCH;

	} else if (!strncmp(argv[2], "bld", COMPARE_STRING_LEN)) {
		printf("bld\n");
		component |= BIT_WRITE_BLD;

	} else if (!strncmp(argv[2], "all", COMPARE_STRING_LEN)) {
		printf("all\n");
		component |= BIT_WRITE_FIP_BIN | BIT_WRITE_ROM_PATCH | BIT_WRITE_BLD;

	} else {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	if (!strncmp(argv[1], "emmc", COMPARE_STRING_LEN)) {
		printf("emmc\n");
		do_bm_update_emmc(component, addr);
	} else if (!strncmp(argv[1], "snor", COMPARE_STRING_LEN)) {
		printf("spinor\n");
	} else if (!strncmp(argv[1], "snand", COMPARE_STRING_LEN)) {
		printf("spinand\n");
	} else {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	return 0;
}

U_BOOT_CMD(
	bm_sd_update, 4, 0, do_bm_sd_update,
	"bm_update_dev  - write images to eMMC/SPI NOR/SPI NAND\n",
	"bm_update_dev dev_type img_type addr - Print a report\n"
	"dev_type : emmc/snor/snand\n"
	"img_type : fip/patch/bld/all\n"
);
