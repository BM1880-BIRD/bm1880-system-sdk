#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/if_alg.h>
#include <linux/socket.h>
#include <string.h>
#include <assert.h>

#ifndef SOL_ALG
#define SOL_ALG 279
#endif

#define SHA1_DIGEST_SIZE	(160 / 8)

int main(void)
{
	int opfd;
	int tfmfd;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "hash",
		.salg_name = "sha1-bitmain-ce"
	};
	char *buf;
	int length = 1024;
	unsigned char hash[SHA1_DIGEST_SIZE] = {0};

	struct af_alg_iv *iv;
	int i;
	int ret;

	printf("malloc size %d\n", length);

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	assert(tfmfd >= 0);

	ret = bind(tfmfd, (struct sockaddr *)&sa, sizeof(sa));
	assert(ret == 0);

	opfd = accept(tfmfd, NULL, 0);
	assert(opfd >= 0);

	buf = malloc(length);
	assert(buf);

	ret = send(opfd, buf, length, 0);
	printf("%d bytes send\n", ret);
	ret = read(opfd, hash, sizeof(hash));
	printf("%d bytes read\n", ret);
	if (ret == -1)
		printf("error code %d\n", errno);

	for (i = 0; i < 16; i++)
		printf("%02x", hash[i]);

	printf("\n");

	close(opfd);
	close(tfmfd);

	return 0;
}
