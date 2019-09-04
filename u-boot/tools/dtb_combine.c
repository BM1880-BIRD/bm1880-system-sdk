#include <stdio.h>
#include <stdint.h>

#define BUF_SIZE		(2 * 1024 * 1024)
#define ALIGN(x, a)		__ALIGN_MASK((x), (typeof(x))(a) - 1)
#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

int main(int argc, char **argv)
{
	int i, n = 0;
	FILE *inputf, *outputf;
	uint8_t buf[BUF_SIZE] = {0};
	uint32_t o_len = 0;

	if (argc <= 1) {
		fprintf(stderr, "No args indicated!\n");
		return -1;
	}

	outputf = stdout;

	for (i = 1; i < argc; ++i) {
		inputf = fopen(argv[i], "r");
		if (!inputf) {
			fprintf(stderr, "open %s error!\n", argv[i]);
			return -1;
		}
		n = fread(buf + o_len, 1, sizeof(buf), inputf);
		if (n <= 0) {
			fprintf(stderr, "reading %s error!\n", argv[i]);
			return -1;
		}
		o_len += ALIGN(n, 16);
		fclose(inputf);
	}

	fwrite(buf, 1, o_len, outputf);

	return 0;
}
