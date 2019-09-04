/*************************************************************************
	> File Name: mcu_wdt.c
	> Author: 
	> Mail: 
	> Created Time: 2019年07月31日 星期三 20时12分32秒
 ************************************************************************/

#include<stdio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define I2C_FILE_NAME "/dev/i2c-1"

typedef struct i2c_update_t {
    unsigned int magic; //"BMCU"
    unsigned short hlen;
    unsigned short seq;

    unsigned short cmd;
    short ret;
    int value;

    short hcrc;
    short dcrc;
    short dlen;
    short reserved;
}i2c_update;

#define I2C_MAGICI     0x55434d42   //BMCU

#define FLASH_BASE     0x8000000
#define FLASH_APP_BASE 0x8008000

#define I2C_RET_OK        0
#define I2C_RET_CMD_FMT   (-1)
#define I2C_RET_DATA_LEN  (-2)

#define ERASE_WIDTH (128)
#define WRITE_SIZE (128 + 4)

int i2c_send_data(int devfd, unsigned char slave_addr, unsigned char *pcmd, int ncmdlen)
{
    int ret;
    struct i2c_msg messages;

    struct i2c_smbus_ioctl_data payload = {
        .read_write = I2C_SMBUS_WRITE,
        .size = I2C_SMBUS_BYTE_DATA,
        .command = 0,
        .data = (void *)pcmd,
    };

    ret = ioctl(devfd, I2C_SLAVE_FORCE, slave_addr);
    if (ret < 0) {
        ret = -errno;
        goto exit;
    }

    ret = ioctl(devfd, I2C_SMBUS, &payload);
    if (ret < 0)
    {
        printf("func:%s ioctl err, ret = %d, errno:%d,%s\n", __func__, ret, errno, strerror(errno));
        ret = -errno;
        goto exit;
    }

exit:
    return ret;
}

int do_i2c_update(int devfd, unsigned char slave_addr, const char *path)
{
    int ret;
    int fd;
    int fsize;
    int count;
    int nr;

    char file_buf[ERASE_WIDTH];
    char buffer[WRITE_SIZE];
    struct stat statbuf;
    stat(path, &statbuf);
    fsize = statbuf.st_size;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open path err: %s\n",path);
        return -1;
    }

    count = fsize/ERASE_WIDTH;
    if (fsize%ERASE_WIDTH) {
        count++;
    }

    for (int i = 0; i < count; ++i)
    {
        nr = read(fd, file_buf, sizeof(file_buf));
        if (nr < 0)
        {
            printf("read err nr: %d count :%d\n",nr, count);
            ret = -1;
            goto ret;
        }

        //printf("1 file_buffer 127:%d\n", file_buf[127]);
        strncpy(buffer, "BMCU", 4);
        memcpy(buffer+4,file_buf, sizeof(file_buf));
        //printf("2 file_buffer 127:%d\n", file_buf[127]);
        //for (i = 0; i < 132; i++)
          //  printf("buffer data[%d]:%d\n", i,buffer[i]);
        ret = i2c_send_data(devfd, slave_addr, buffer, sizeof(buffer));
        if (ret < 0)
        {
            printf("i2c send data error!\n");
            goto ret;
        }
    }

ret:
    close(fd);
    return ret;
}


int main(int argc, char *argv[])
{
    int ret;
    char *path = NULL;
    int devfd;
    char devfile[32];
    int ch;
    int options_index;
    int bus = 0;
    int slave_addr = 0;


    static const struct option long_options[] = {
        {"bus", required_argument, NULL, 'b'},
        {"addr", required_argument, NULL, 'a'},
        {"file", required_argument, NULL, 'f'},
        {"cmd", required_argument, NULL, 'c'},
        {NULL,0, NULL, 0}
    };

    while ((ch = getopt_long(argc, argv, "b:a:f:dc:", long_options, &options_index)) != EOF)
    {
        switch (ch) {
            case 'b':
                bus = atoi(optarg);
                break;
            case 'a':
                slave_addr = atoi(optarg);
                break;
            case 'f':
                path = optarg;
                break;
            default:
                printf("arg:%s\n", argv[optind]);
                break;
        }
    }

    sprintf(devfile, "/dev/i2c-%d", bus);
    printf("path:%s\n",path);
    if ((devfd = open(devfile, O_RDWR)) < 0) {
        perror("");
        printf("unable to open i2c control file : %s\n", devfile);
        return -1;
    }

    printf("devfile:%s slave_addr:%d\n",devfile, slave_addr);

    ret = do_i2c_update(devfd, slave_addr, path);

    return ret;
}
