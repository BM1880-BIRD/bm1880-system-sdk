#include <stdio.h>
#include <string.h>
#include "include/libbm1880sn.h"

void main(void)
{
	char bm1880_sn[33];

	if (bm1880sn_get_sn(bm1880_sn, sizeof(bm1880_sn)-1) == 0)
		printf("BM1880 SN is %s\n", bm1880_sn);
	else
		printf("Read BM1880 SN failed.\n");
}
