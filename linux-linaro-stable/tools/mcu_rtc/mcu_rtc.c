#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include "devmem.h"

enum {
	UNKNOWN = 0,
	BM1682,
	BM1880,
	BM1684,
};

int main(int argc, char *argv[])
{
	struct timeval tv;
	int fd, model = UNKNOWN;
	char cpuinfo[512];

	fd = open("/proc/cpuinfo", O_RDONLY);
	if (fd < 0) {
		printf("failed to get cpu info %d\n", errno);
		return -EFAULT;
	}
	memset(cpuinfo, 0, sizeof(cpuinfo));
	read(fd, cpuinfo, sizeof(cpuinfo) - 1);
	if (strstr(cpuinfo, "bm1682"))
		model = BM1682;
	else if (strstr(cpuinfo, "bm1880"))
		model = BM1880;
	else if (strstr(cpuinfo, "bm1684"))
		model = BM1684;

	switch (model) {
	case BM1682:
		{
			unsigned int date_hex;
			int ret;

			printf("chip model: bm1682\n");
			date_hex = devmem_readl(0x50008240);
			if (date_hex == 0)
				return -EINVAL;

			gettimeofday(&tv, NULL);
			printf("change time from %ld to %ld\n", tv.tv_sec, date_hex);
			tv.tv_sec = (time_t)date_hex;
			ret = settimeofday(&tv, NULL);
			if (ret)
				printf("change time failed %d\n", errno);
		}
		break;
	case BM1880:
		printf("chip model: bm1880\n");
		break;
	case BM1684:
		printf("chip model: bm1684\n");
		break;
	default:
		printf("unknown chip model\n");
		break;
	}

	return 0;
}
