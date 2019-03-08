#include <stdlib.h>
#include <common.h>
#include <command.h>
#include <bm_utask.h>

__weak int bm_usb_polling(void)
{
	return 0;
}

static int do_bm_utask(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	printf("\nstart usb task!\n");
	bm_usb_polling();

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bm_utask, 2, 0, do_bm_utask,
	"bootloader control block command",
	"bm_bcb <interface> <dev> <varname>\n"
);

