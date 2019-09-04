/*
 * cmd_ddr.c - just like `ddr` command
 *
 * Copyright (c) 2008 Bitmain Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <config.h>
#include <common.h>
#include <command.h>

extern int g_ddr_size;

int do_ddr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *var = NULL;
	char size_s[10] = { 0 };
	int size = 0;

	if (argc > 2)
		return CMD_RET_USAGE;

	var = argv[1];
	size = g_ddr_size;//get_ddr_size();
	snprintf(size_s, 10, "%d", size);
	setenv(var, size_s);
	printf("ddr size is %s GB\n", size_s);

	return 0;
}

U_BOOT_CMD(
	ddr, 2, 1, do_ddr,
	"display ddr size (GB)",
	"<var>\n"
	"    - display ddr size and set it to <var>\n"
);
