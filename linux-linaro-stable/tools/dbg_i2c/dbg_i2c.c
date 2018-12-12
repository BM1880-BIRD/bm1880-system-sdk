#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#if 1
#define LOGV(args...) printf(args)
#else
#define LOGV(args...)
#endif

static int i2c_file; // I2C device fd
static unsigned char dbg_i2c_addr;

#define DBG_I2C_DATA_LEN 128

enum {
	ADDRW_8,
	ADDRW_16,
	ADDRW_24,
	ADDRW_32,
	ADDRW_40,
	ADDRW_48,
};

enum {
	DATALEN_1BYTES,
	DATALEN_2BYTES,
	DATALEN_4BYTES,
	DATALEN_ANYBYTES,
};

static int dbgi2c_set_addrwidth(int addr_width)
{
	struct i2c_rdwr_ioctl_data *i2c_data;
	struct i2c_msg *msgs0;
	char cmd;
	int ret;

	i2c_data = (struct i2c_rdwr_ioctl_data *)malloc(sizeof(struct i2c_rdwr_ioctl_data));
	i2c_data->msgs = (struct i2c_msg *)malloc(2 * sizeof(struct i2c_msg));

	switch (addr_width) {
	case ADDRW_8:
		cmd = 0x20;
		break;
	case ADDRW_16:
		cmd = 0x21;
		break;
	case ADDRW_24:
		cmd = 0x22;
		break;
	case ADDRW_32:
		cmd = 0x23;
		break;
	case ADDRW_40:
		cmd = 0x24;
		break;
	case ADDRW_48:
		cmd = 0x25;
		break;
	default:
		cmd = 0x24;
		break;
	}

	msgs0 = &i2c_data->msgs[0];
	i2c_data->nmsgs  = 1;
	msgs0->len = 1;
	msgs0->flags = 0; // write
	msgs0->addr = dbg_i2c_addr;
	msgs0->buf = &cmd;
	ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
	if (ret < 0)
		printf("dbgi2c_set_addrwidth failed: %d %d\n", ret, errno);

	free(i2c_data->msgs);
	free(i2c_data);
	return ret;
}

int dbgi2c_write32(unsigned long addr, unsigned int value)
{
	struct i2c_rdwr_ioctl_data *i2c_data;
	struct i2c_msg *msgs0;
	int header_size = 0;
	char header_body[32] = {0};
	int ret = 0;

	i2c_data = (struct i2c_rdwr_ioctl_data *)malloc(sizeof(struct i2c_rdwr_ioctl_data));
	i2c_data->msgs = (struct i2c_msg *)malloc(sizeof(struct i2c_msg));

	msgs0 = &i2c_data->msgs[0];
	i2c_data->nmsgs = 1;
	header_body[0] = 0x12; // four byte per trans
	header_body[1] = addr & 0xff;
	header_body[2] = (addr >> 8) & 0xff;
	header_body[3] = (addr >> 16) & 0xff;
	header_body[4] = (addr >> 24) & 0xff;
	if (addr & 0xff00000000) {
		header_body[5] = addr >> 32;
		header_size  = 6;
		dbgi2c_set_addrwidth(ADDRW_40);
	} else {
		header_size  = 5;
		dbgi2c_set_addrwidth(ADDRW_32);
	}
	*(unsigned int *)(header_body + header_size) = value;

	msgs0->len = header_size + 4;
	msgs0->flags = 0; // write
	msgs0->addr = dbg_i2c_addr;
	msgs0->buf = header_body;

	ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
	if (ret < 0)
		printf("dbgi2c_wrte32 failed: %d %d\n", ret, errno);

	free(i2c_data->msgs);
	free(i2c_data);
	return ret;
}

int dbgi2c_read32(unsigned long addr, unsigned int *value)
{
	struct i2c_rdwr_ioctl_data *i2c_data;
	struct i2c_msg *msgs0;
	struct i2c_msg *msgs1;
	int header_size = 0;
	char header_body[32] = {0};
	int ret = 0;

	i2c_data = (struct i2c_rdwr_ioctl_data *)malloc(sizeof(struct i2c_rdwr_ioctl_data));
	i2c_data->msgs = (struct i2c_msg *)malloc(2 * sizeof(struct i2c_msg));

	msgs0 = &i2c_data->msgs[0];
	msgs1 = &i2c_data->msgs[1];
	i2c_data->nmsgs = 2;
	header_body[0] = 0x12; // four byte per trans
	header_body[1] = addr & 0xff;
	header_body[2] = (addr >> 8) & 0xff;
	header_body[3] = (addr >> 16) & 0xff;
	header_body[4] = (addr >> 24) & 0xff;
	if (addr & 0xff00000000) {
		header_body[5] = addr >> 32;
		header_size  = 6;
		dbgi2c_set_addrwidth(ADDRW_40);
	} else {
		header_size  = 5;
		dbgi2c_set_addrwidth(ADDRW_32);
	}

	msgs0->len = header_size;
	msgs0->flags = 0; // write
	msgs0->addr = dbg_i2c_addr;
	msgs0->buf = header_body;

	msgs1->len = 4;
	msgs1->flags = I2C_M_RD;
	msgs1->addr = dbg_i2c_addr;
	msgs1->buf = (char *)value;

	ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
	if (ret < 0)
		printf("dbgi2c_read32 failed %d %d\n", ret, errno);

	free(i2c_data->msgs);
	free(i2c_data);
	return ret;
}

int dbgi2c_write(unsigned long addr, char *buf, int len)
{
	struct i2c_rdwr_ioctl_data *i2c_data;
	struct i2c_msg *msgs0;
	int header_size = 0;
	char header_body[DBG_I2C_DATA_LEN + 32] = {0};
	int i, j, k;
	int ret = 0;

	i2c_data = (struct i2c_rdwr_ioctl_data *)malloc(sizeof(struct i2c_rdwr_ioctl_data));
	i2c_data->msgs = (struct i2c_msg *)malloc(sizeof(struct i2c_msg));

	msgs0 = &i2c_data->msgs[0];
	i2c_data->nmsgs = 1;
	header_body[0] = 0x18; //any byte per trans
	header_body[1] = addr & 0xff;
	header_body[2] = (addr >> 8) & 0xff;
	header_body[3] = (addr >> 16) & 0xff;
	header_body[4] = (addr >> 24) & 0xff;
	if (addr & 0xff00000000) {
		header_body[5] = addr >> 32;
		header_size  = 6;
		dbgi2c_set_addrwidth(ADDRW_40);
	} else {
		header_size  = 5;
		dbgi2c_set_addrwidth(ADDRW_32);
	}
	msgs0->len = DBG_I2C_DATA_LEN + header_size;
	msgs0->flags = 0; // write
	msgs0->addr = dbg_i2c_addr;
	msgs0->buf = header_body;

	if (len > DBG_I2C_DATA_LEN) {
		j =  len / DBG_I2C_DATA_LEN;
		k =  len % DBG_I2C_DATA_LEN;
		for (i = 0; i < j; i++) {
			memcpy(header_body + header_size, buf + DBG_I2C_DATA_LEN * i, DBG_I2C_DATA_LEN);
			ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
			if (ret < 0) {
				printf("dbgi2c_write %d/%d failed: %d %d\n", i, j, ret, errno);
				goto out;
			}
			// TODO: cross 4GB boundary is not supported
			*(unsigned int *)(header_body + 1) += DBG_I2C_DATA_LEN;
		}
		if (k) {
			memcpy(header_body + header_size, buf + DBG_I2C_DATA_LEN*i, k);

			// the length should be multiple of 4, otherwise the last several bytes won't send
			if (k % 4)
				k += (4 - k % 4);

			msgs0->len = k + header_size;
			ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
			if (ret < 0) {
				printf("dbgi2c_write last block failed %d %d\n", ret, errno);
				goto out;
			}
		}
	} else {
		memcpy(header_body + header_size, buf, len);
		if (len % 4)
			len += (4 - len % 4);
		msgs0->len = len + header_size;
		ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
		if (ret < 0) {
			printf("dbgi2c_write failed %d %d\n", ret, errno);
			goto out;
		}
	}

out:
	free(i2c_data->msgs);
	free(i2c_data);
	return ret;
}


int dbgi2c_read(unsigned long addr, char *buf, int len)
{
	struct i2c_rdwr_ioctl_data *i2c_data;
	struct i2c_msg *msgs0;
	struct i2c_msg *msgs1;
	int header_size = 0;
	char header_body[DBG_I2C_DATA_LEN + 32] = {0};
	int i, j, k;
	int ret = 0;

	i2c_data = (struct i2c_rdwr_ioctl_data *)malloc(sizeof(struct i2c_rdwr_ioctl_data));
	i2c_data->msgs = (struct i2c_msg *)malloc(2 * sizeof(struct i2c_msg));

	msgs0 = &i2c_data->msgs[0];
	msgs1 = &i2c_data->msgs[1];
	i2c_data->nmsgs = 2;
	header_body[0] = 0x18; // any byte per trans
	header_body[1] = addr & 0xff;
	header_body[2] = (addr >> 8) & 0xff;
	header_body[3] = (addr >> 16) & 0xff;
	header_body[4] = (addr >> 24) & 0xff;
	if (addr & 0xff00000000) {
		header_body[5] = addr >> 32;
		header_size  = 6;
		dbgi2c_set_addrwidth(ADDRW_40);
	} else {
		header_size  = 5;
		dbgi2c_set_addrwidth(ADDRW_32);
	}

	msgs0->len = header_size;
	msgs0->flags = 0; // write
	msgs0->addr = dbg_i2c_addr;
	msgs0->buf = header_body;

	msgs1->flags = I2C_M_RD;
	msgs1->addr = dbg_i2c_addr;

	if (len > DBG_I2C_DATA_LEN) {
		j =  len / DBG_I2C_DATA_LEN;
		k =  len % DBG_I2C_DATA_LEN;

		msgs1->len = DBG_I2C_DATA_LEN;
		for (i = 0; i < j; i++) {
			msgs1->buf = buf + i * DBG_I2C_DATA_LEN;
			ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
			if (ret < 0) {
				printf("dbgi2c_read %d/%d failed: %d %d\n", i, j, ret, errno);
				goto out;
			}
			// TODO: cross 4GB boundary is not supported
			*(unsigned int *)(header_body+1) += DBG_I2C_DATA_LEN;
		}
		if (k) {
			msgs1->len = k;
			msgs1->buf = buf + i * DBG_I2C_DATA_LEN;
			ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
			if (ret < 0) {
				printf("dbgi2c_read last block failed %d %d\n", ret, errno);
				goto out;
			}
		}
	} else {
		msgs1->buf = buf;
		msgs1->len = len;
		ret = ioctl(i2c_file, I2C_RDWR, (unsigned long)i2c_data);
		if (ret < 0) {
			printf("dbgi2c_read failed %d %d\n", ret, errno);
			goto out;
		}
	}

out:
	free(i2c_data->msgs);
	free(i2c_data);
	return ret;
}

long int xstrtol(const char *s)
{
	long int tmp;

	errno = 0;
	tmp = strtol(s, NULL, 0);
	if (!errno)
		return tmp;

	if (errno == ERANGE)
		fprintf(stderr, "Bad integer format: %s\n",  s);
	else
		fprintf(stderr, "Error while parsing %s: %s\n", s,
				strerror(errno));

	exit(EXIT_FAILURE);
}

int main(int argc, char * const argv[])
{
	int ret = 0, option, i2c_port = -1, is_read = 0, length = 0;
	const char *file_str = NULL;
	unsigned long address = 0;
	unsigned int value = 0;
	char i2c_file_name[20];

	if (argc == 1) {
		printf("usage:\n\t"
			"dbg_i2c -d 5 -s 0x26 -f fip.bin -r(w) 0x410000000 [-l 0x1000]\n\t"
			"dbg_i2c -d 5 -s 0x26 -r(w) 0x4100FFFFC -v 0xEAEAEAEA\n\t"
			"dbg_i2c -d 5 -s 0x26 -f fip.bin -r(w) 0x10100000 [-l 0x1000]\n\t"
			"dbg_i2c -d 5 -s 0x26 -r(w) 0x101FFFFC -v 0xEAEAEAEA\n");
		return -1;
	}

	while ((option = getopt(argc, argv, "d:s:f:r:w:v:l:")) != -1) {
		switch (option) {
		case 'd':
			i2c_port = xstrtol(optarg);
			break;
		case 's':
			dbg_i2c_addr = xstrtol(optarg);
			break;
		case 'f':
			file_str = strdup(optarg);
			if (!file_str) {
				printf("please give valid file name\n");
				return -EFAULT;
			}
			break;
		case 'r':
		case 'w':
			address = xstrtol(optarg);
			is_read =  option == 'r' ? 1 : 0;
			break;
		case 'l':
			length = xstrtol(optarg);
			break;
		case 'v':
			value = xstrtol(optarg);
			break;
		}
	}

	printf("args: -d %d -s 0x%x -f %s -%c 0x%lx -v 0x%x -l 0x%x\n",
		i2c_port, dbg_i2c_addr, file_str, is_read ? 'r' : 'w', address, value, length);
	if (i2c_port < 0 || !dbg_i2c_addr || (file_str && is_read && !length)) {
		printf("please give all arguments we need\n");
		return -EFAULT;
	}

	snprintf(i2c_file_name, sizeof(i2c_file_name), "/dev/i2c-%d", i2c_port);
	i2c_file = open(i2c_file_name, O_RDWR);
	if (i2c_file < 0) {
		printf("open I2C device node failed: %d\n", errno);
		return -ENODEV;
	}

	if (file_str) {
		if (is_read) {
			int fd = open(file_str, O_CREAT | O_RDWR, 0666);
			char *buf = malloc(length);
			int count = 0;

			if (!buf) {
				printf("alloc reading buf failed: %d\n", errno);
				goto out_r;
			}
			if (fd < 0) {
				printf("open file failed: %d\n", errno);
				goto out_r;
			}
			if (dbgi2c_read(address, buf, length) >= 0) {
				while (count < length) {
					ret = write(fd, buf + count, length - count);
					if (ret < 0) {
						printf("write file %d/%d failed: %d %d\n", count, length, ret, errno);
						goto out_r;
					}
					count += ret;
				}
			}
out_r:
			free(buf);
			close(fd);
			printf("read %d bytes done\n", length);
		} else {
			int fd = open(file_str, O_RDONLY);
			char *buf;
			struct stat statbuf;
			int count = 0;

			if (fd < 0) {
				printf("open file failed: %d\n", errno);
				goto out_w;
			}
			stat(file_str, &statbuf);
			if (length == 0)
				length = statbuf.st_size;
			buf = malloc(length);
			if (!buf) {
				printf("alloc reading buf failed: %d\n", errno);
				goto out_w;
			}
			while (count < length) {
				ret = read(fd, buf + count, length - count);
				if (ret < 0) {
					printf("read file %d/%d failed: %d %d\n", count, length, ret, errno);
					goto out_w;
				}
				count += ret;
			}
			dbgi2c_write(address, buf, length);
out_w:
			free(buf);
			close(fd);
			printf("write %d bytes done\n", length);
		}
	} else {
		if (is_read) {
			if (dbgi2c_read32(address, &value) >= 0)
				printf("0x%lx: 0x%x\n", address, value);
		} else {
			dbgi2c_write32(address, value);
			printf("done\n");
		}
	}

	close(i2c_file);
	return 0;
}
