#include <stdlib.h>
#include <common.h>
#include <command.h>
#include <bm_reboot.h>

__weak void software_root_reset(void)
{
}

static int do_bm_reboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	printf("\nbm reboot!\n");
	software_root_reset();

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bm_reboot,	2,	0,	do_bm_reboot,
	"bootloader control block command",
	"bm_bcb <interface> <dev> <varname>\n"
);

