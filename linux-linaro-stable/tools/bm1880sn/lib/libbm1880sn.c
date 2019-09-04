#include <stdio.h>
#include <string.h>
#include "devmem.h"

#define BM1880_SN_ADDR 0x04000360

int bm1880sn_get_sn(char *sn_str, size_t count)
{
	uint32_t sn[4];
	char tmp_str[33];
	uint32_t verify = 1 << 31;
	int i;

	for (i = 0; i < 4; i++) {
		sn[i] = devmem_readl(BM1880_SN_ADDR+(i<<2));
		verify &= sn[i];
	}

	if (verify)
		sprintf(tmp_str, "%08X%08X%08X%08X", sn[0], sn[1], sn[2], sn[3]);
	else
		sprintf(tmp_str, "DEADBEEFDEADBEEF");

	strncpy(sn_str, tmp_str, count > 32 ? 32 : count);
	return verify ? 0 : -1;
}
