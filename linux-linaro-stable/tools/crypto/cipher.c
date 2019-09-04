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

#define INPLACE

//#define xmalloc	valloc
#define xmalloc			malloc
#define offset			(16 * 10)
#define length			(4096 + 16 * 128)

int main(void)
{
	int opfd;
	int tfmfd;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "skcipher",
		.salg_name = "cbc(aes)-bitmain-ce"
	};
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	char cbuf[CMSG_SPACE(4) + CMSG_SPACE(20)] = {0};
	char *buf;
	struct af_alg_iv *iv;
	struct iovec iov;
	int i;
	int ret;

	printf("malloc size %d\n", length);

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	assert(tfmfd >= 0);

	ret = bind(tfmfd, (struct sockaddr *)&sa, sizeof(sa));
	assert(ret == 0);

	ret = setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY,
		   "\x06\xa9\x21\x40\x36\xb8\xa1\x5b"
		   "\x51\x2e\x03\xd5\x34\x12\x00\x06", 16);
	assert(ret == 0);

	opfd = accept(tfmfd, NULL, 0);
	assert(opfd >= 0);

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(__u32 *)CMSG_DATA(cmsg) = ALG_OP_ENCRYPT;

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(20);
	iv = (void *)CMSG_DATA(cmsg);
	iv->ivlen = 16;
	memcpy(iv->iv, "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30"
	       "\xb4\x22\xda\x80\x2c\x9f\xac\x41", 16);

	iov.iov_len = length;
	iov.iov_base = xmalloc(iov.iov_len + offset);
	iov.iov_base = (unsigned char *)iov.iov_base + offset;
#ifdef INPLACE
	buf = iov.iov_base;
#else
	buf = xmalloc(iov.iov_len);
#endif
	if (iov.iov_base == NULL || buf == NULL) {
		printf("malloc %lu bytes failed\n", iov.iov_len);
		return -1;
	}

	if (buf == iov.iov_base)
		printf("IN PLACE\n");

	printf("malloc %lu bytes success\n", iov.iov_len);

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ret = sendmsg(opfd, &msg, 0);
	printf("%d bytes send\n", ret);
	ret = read(opfd, buf, iov.iov_len);
	printf("%d bytes read\n", ret);
	if (ret == -1)
		printf("error code %d\n", errno);

	for (i = 0; i < 16; i++)
		printf("%02x", (unsigned char)buf[i]);

	printf("\n");

	close(opfd);
	close(tfmfd);

	return 0;
}
