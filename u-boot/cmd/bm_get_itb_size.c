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

static int do_get_itb_size(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *addr;
	uint32_t size = 0;
	uint32_t img_blk_cnt = 0;
	char dst[16];
	struct fdt_header *fh;

	if (argc != 3) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	addr = (char *)simple_strtol(argv[2], NULL, 16);

	if (!addr) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	fh = (struct fdt_header *)addr;

	pr_debug("magiclit 0x%x\n", htonl(fh->magic));

	if (htonl(fh->magic) != FDT_MAGIC) {
		printf("Wrong fdt image:0x%x\n", htonl(fh->magic));
		return 1;
	}

	pr_debug("totalsizel 0x%x\n", htonl(fh->totalsize));
	size = htonl(fh->totalsize);

	img_blk_cnt = DIV_ROUND_UP(size, 512);

	sprintf(dst, "%x", img_blk_cnt);

	setenv(argv[1], dst);
	return 0;
}

U_BOOT_CMD(
	get_itb_size, 4, 0, do_get_itb_size,
	"get_itb_size  - parse itb file and get file size\n",
	"name addr - parse itb file from addr 'addr' and set file size to 'name'\n"
);
