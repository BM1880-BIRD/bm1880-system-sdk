/*
 ** catch version num in userspace
 ** date:2019.07.09
 ** yuming.liang@bitmain.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include "debug.h"
#include "devmem.h"

int debug = DEBUG_LEVEL_ERR;

char *uboot_cat = "U-Boot 2017.07-00009-g683252e (May 28 2019 - 16:29:10 +0800)";
#define uboot_cat_len strlen(uboot_cat)
char *BL_cat = "Built :                                      ";
#define BL_cat_len strlen(BL_cat)
char *BLv_cat = "v1.4                                        ";
#define BLv_cat_len strlen(BLv_cat)

unsigned long ver_addr_start = 0x44000000;
#define LENALL (1024*896)
static char read_dev[LENALL];
void get_devmem(void)
{
	void *virt_addr;
	unsigned int i;
	//reset operation
	devmem_writel(0x50008014, 0xff7fffff);
	devmem_writel(0x50008014, 0xffffffff);
	//read operationd
	virt_addr = devm_map(ver_addr_start, LENALL);
	if (virt_addr == NULL) {
		ERR("cat version map failed test\n");
		return;
	}
	memset(read_dev, 0, sizeof(read_dev));
	for (i = 0; i < LENALL; i++) {
		read_dev[i] = *(char *)(virt_addr+i);
		//if (read_dev[i] < 128 && read_dev[i] > 31 || read_dev[i] == 10)
			//printf("%c", read_dev[i]);
	}

	devm_unmap(virt_addr, LENALL);
}
char *cat_Ubootmes(char *cat, unsigned int len)
{
	char check_buf[len], checknum;
	char *mes;
	unsigned int i;

	memset(check_buf, 0, sizeof(check_buf));
	checknum = 0;
	for (i = 0; i < LENALL-len; i++) {
		if (read_dev[i] == cat[0]) {
			int k = 0;

			for (int j = 0; j < len; j++) {
				if (read_dev[i + j] == cat[j] && read_dev[i + j] != ' ')
					k++;
			}
			if (k > checknum) {
				checknum = k;
				strncpy(check_buf, &read_dev[i], len);
			}
		}
	}
	mes = check_buf;
	puts(mes);
	return mes;
}
char *cat_BLmes(char *cat, unsigned int len, unsigned int num)
{
	char check_buf[len];
	char *mes;
	unsigned int i;

	memset(check_buf, 0, sizeof(check_buf));
	for (i = 0; i < LENALL-len; i++) {
		if (read_dev[i] == cat[0]) {
			int k = 0;

			for (int j = 0; j < len; j++) {
				if (read_dev[i + j] == cat[j] && read_dev[i + j] != ' ')
					k++;
			}
			if (k == num) {
				k = 0;
				strncpy(check_buf, &read_dev[i], len);
				mes = check_buf;
				puts(mes);
				memset(check_buf, 0, sizeof(check_buf));
			}
		}
	}
	return mes;
}
int main(int argc, char *argv[])
{
	if (argv[1]) {
		if (strstr(argv[1], "uboot")) {
			get_devmem();
			cat_Ubootmes(uboot_cat, uboot_cat_len);
		} else if (strstr(argv[1], "bl")) {
			get_devmem();
			cat_BLmes(BL_cat, BL_cat_len, 6);
			//cat_BLmes(BLv_cat, BLv_cat_len, 4);
		} else if (strstr(argv[1], "all")) {
			get_devmem();
			cat_Ubootmes(uboot_cat, uboot_cat_len);
			cat_BLmes(BL_cat, BL_cat_len, 6);
		} else if (strstr(argv[1], "--help")) {
			puts("Options:");
			puts("	uboot,      will output uboot version");
			puts("	bl   ,      will output bl1, bl2 and bl3 version");
			puts("	all  ,      will output uboot and bl version\n");
			puts("Failed : need sudo");
			puts("Example:sudo ./fip_ops uboot");
		} else {
			puts("fip_ops --help  can help you know how to operate");
		}
	} else {
		puts("fip_ops --help  can help you know how to operate");
	}
	return 0;
}
