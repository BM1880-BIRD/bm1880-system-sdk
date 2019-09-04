#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <limits.h>
#include "testboard.h"


/****************************
 *0x00   ready flag*
 *0x01   pcie*
 *0x02   sd card
 *0x03   slot id
 *0x04   dbg_i2c_add*
 ******************************/

static	int i2c_file;
static	int slave_addr;
static	int debug_enable = -1;

#define DBG_MSG(fmt, args...) \
do { \
	if (debug_enable == 1) \
		printf("Testboard %s" fmt, __func__, ##args); \
} while (0)


int i2c_read_byte(int file, unsigned char addr, unsigned char reg,
		unsigned char *val) {
	struct i2c_rdwr_ioctl_data i2c_data;
	struct i2c_msg messages[2];
	int ret;
	unsigned char inbuf, outbuf;

	outbuf = reg;
	i2c_data.msgs = messages;
	i2c_data.nmsgs = 2;

	messages[0].addr = addr;
	messages[0].flags = 0;
	messages[0].len = sizeof(outbuf);
	messages[0].buf = &outbuf;

	messages[1].addr = addr;
	messages[1].flags = I2C_M_RD;
	messages[1].len = sizeof(inbuf);
	messages[1].buf = &inbuf;

	ret = ioctl(file, I2C_RDWR, &i2c_data);
	if (ret < 0) {
		DBG_MSG("i2c read failed\n");
		return ret;
	}
	*val = inbuf;
	return 0;
}


int i2c_write_byte(int file, unsigned char addr, unsigned char reg,
	unsigned char *val) {
	struct i2c_rdwr_ioctl_data i2c_data;
	struct i2c_msg messages;
	int ret;
	unsigned char outbuf[2];

	outbuf[0] = reg;
	outbuf[1] = *val;
	i2c_data.msgs = &messages;
	i2c_data.nmsgs = 1;

	messages.addr = addr;
	messages.flags = 0;
	messages.len = sizeof(outbuf);
	messages.buf = outbuf;

	ret = ioctl(file, I2C_RDWR, &i2c_data);
	if (ret < 0) {
		DBG_MSG("i2c write failed\n");
		return ret;
	}
	return 0;
}


int file_read(const char *path, char *buf, int len)
{
	int fd = 0, ret;

	if (access(path, F_OK) != 0) {
		DBG_MSG("%s not exist\n", path);
		return -1;
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		DBG_MSG("open %s failed\n", path);
		close(fd);
		return -1;
	}

	ret = read(fd, buf, len);
	if (ret < 0 || ret != len) {
		perror("read error");
		close(fd);
		return -1;
	}

	close(fd);
}

int file_write(const char *path, char *buf, int len)
{
	int fd = 0, ret;

	if (access(path, F_OK) != 0) {
		DBG_MSG("%s not exist\n", path);
		return -1;
	}
	fd = open(path, O_RDWR);
	if (fd < 0) {
		DBG_MSG("open %s failed\n", path);
		close(fd);
		return -1;
	}

	ret = write(fd, buf, len);
	if (ret < 0 || ret != len) {
		perror("write error");
		close(fd);
		return -1;
	}

	close(fd);
}


int gpio_read(unsigned int gpio_num, unsigned int *gpio_val)
{
	char cmd_val[128];
	char cmd_export[128];
	char cmd_direction[128];
	unsigned char str_val;
	long int int_val;

	snprintf(cmd_val, sizeof(cmd_val),
		"/sys/class/gpio/gpio%d/value", gpio_num);
	snprintf(cmd_export, sizeof(cmd_export),
		"echo %d > /sys/class/gpio/export", gpio_num);
	snprintf(cmd_direction, sizeof(cmd_direction),
		"echo in > /sys/class/gpio/gpio%d/direction", gpio_num);
	if (access(cmd_val, F_OK) != 0) {
		system(cmd_export);
		system(cmd_direction);
	}

	if (file_read(cmd_val, &str_val, 1)) {
		DBG_MSG("read value fail\n");
		return -1;
	}
	errno = 0;    /* To distinguish success/failure after call */
	int_val = strtol(&str_val, NULL, 0);

	/* Check for various possible errors */
	if ((errno == ERANGE && (int_val == LONG_MAX || int_val == LONG_MIN))
		   || (errno != 0 && int_val == 0)) {
		perror("gpio_read error, strtol");
		return -1;
	}

	if (int_val != 0 && int_val != 1) {
		DBG_MSG("read value error, val=%d\n", int_val);
		return -1;
	}
	*gpio_val = (unsigned int)int_val;
	DBG_MSG("gpio%d  val=%d\n", gpio_num, *gpio_val);
	return *gpio_val;
}

int wait_reg_timeout(unsigned char reg, unsigned char val, int timeout)
{
	unsigned char status;

	while (timeout--) {
		i2c_read_byte(i2c_file, slave_addr, reg, &status);
		if (val == status)
			return 0;
		sleep(1);
	}
	DBG_MSG("wait reg 0x%x  val 0x%x timeout\n", reg, val);
	return -1;
}

int sdcard_test(void)
{
	char rd_buf[128];

	if (file_read("/dev/mmcblk1p1", rd_buf, sizeof(rd_buf))) {
		DBG_MSG("sdcard read failed\n");
		return -1;
	}
	DBG_MSG("sdcard read success\n");
	return 0;
}

int pcie_test(void)
{
	FILE *fd;
	int ret;
	char buf[512];

	fd = popen("lspci", "r");
	if (fd == NULL) {
		perror("popen error\n");
		return -1;
	}
	ret = fread(buf, sizeof(char), sizeof(buf), fd);
	if (ret < 0) {
		perror("fread error\n");
		return -1;
	}
	pclose(fd);

	if (strstr(buf, "1684") == NULL) {
		DBG_MSG("pcie test failed, buf=%s\n", buf);
		return -1;
	}

	DBG_MSG("pcie test ok!\n");
	return 0;
}


int slotid_dbgi2caddr_test(void)
{
	int ret = 0, i = 0;
	unsigned char reg, val;
	unsigned int gpio_val;

	/*get debug_i2c addr*/
	reg = 4;
	val = 0;
	for (i = 0; i < 3; i++) {
		ret = gpio_read(i+485, &gpio_val);
		if (ret < 0) {
			DBG_MSG("gpio%d read failed\n", i);
			return -1;
		}
		val |= (unsigned char)(gpio_val << i);
	}
	i2c_write_byte(i2c_file, slave_addr, reg, &val);
	DBG_MSG("read dbgi2c_addr=0x%x\n", val);

	/*get slot id*/
	reg = 3;
	val = 0;
	gpio_val = 0;
	ret = gpio_read(427, &gpio_val);
	if (ret < 0) {
		DBG_MSG("gpio427 read failed\n");
		return -1;
	}
	val = (unsigned char)(gpio_val << 1);
	ret = gpio_read(429, &gpio_val);
	if (ret < 0) {
		DBG_MSG("gpio429 read failed\n", i);
		return -1;
	}
	val |= (unsigned char)gpio_val;

	i2c_write_byte(i2c_file, slave_addr, reg, &val);
	DBG_MSG("read slot_id=0x%x\n", val);
}


int main(int argc, char const *argv[])
{
	int i, ret, i2c_port;
	unsigned char reg, val;
	unsigned int gpio_val;
	char i2c_portname[64];
	pthread_t pid_uart1, pid_uart2;

	if (argc != 3) {
		printf("Usage: %s i2cport i2c_slave_addr\n", argv[0]);
		return -1;
	}

	ret = file_write(
		"/sys/class/pinmux/bm_pinmux/i2c2", "1", 1);
	if (ret < 0) {
		printf("Testboard set pinmux gpio to i2c2 failed\n");
		goto out;
	}

	i2c_port = strtol(argv[1], NULL, 0);
	slave_addr = strtol(argv[2], NULL, 0);

	snprintf(i2c_portname, sizeof(i2c_portname), "/dev/i2c-%d", i2c_port);
	i2c_file = open(i2c_portname, O_RDWR);
	if (i2c_file < 0) {
		printf("Testboard open i2c device failed, %d\n", errno);
		goto out;
	}

	/*wait mcu ready*/
	ret = wait_reg_timeout(0x0, 0xaa, 5);
	if (ret < 0) {
		printf("Testboard MCU not ready\n");
		goto out;
	}
	DBG_MSG("Start testboard test, slave:%x!\n", slave_addr);
	debug_enable = 1;
	sleep(5);
	/*set flag ready*/
	reg = 0;
	val = 1;
	i2c_write_byte(i2c_file, slave_addr, reg, &val);

	/*sdcard*/
	if (sdcard_test() >= 0) {
		reg = 2;
		val = 1;
		i2c_write_byte(i2c_file, slave_addr, reg, &val);
		DBG_MSG("sdcard test ok!");
	}

	/*pcie test*/
	if (!pcie_test()) {
		reg = 1;
		val = 1;
		i2c_write_byte(i2c_file, slave_addr, reg, &val);
		DBG_MSG("pcie test ok!");
	}

	/*uart test*/
	pthread_create(&pid_uart1, NULL, thread_fn, "/dev/ttyS1");
	pthread_create(&pid_uart2, NULL, thread_fn, "/dev/ttyS2");

	/*slotid dbgi2caddr*/
	ret = file_write(
		"/sys/class/pinmux/bm_pinmux/pwm0", "0", 1);
	if (ret < 0) {
		DBG_MSG("set pinmux pwm0 to gpio failed\n");
		goto out;
	}

	ret = file_write(
		"/sys/class/pinmux/bm_pinmux/fan", "0", 1);
	if (ret < 0) {
		DBG_MSG("set pinmux fan to gpio failed\n");
		goto out;
	}

	if (wait_reg_timeout(0x5, 0xaa, 180) == 0) {
		slotid_dbgi2caddr_test();
		if (wait_reg_timeout(0x5, 0x55, 180) == 0)
			slotid_dbgi2caddr_test();
	}

	i = 0;
	while (i++ < 180)
		sleep(1);

out:
	ret = file_write(
		"/sys/class/pinmux/bm_pinmux/i2c2", "0", 1);
	if (ret < 0)
		DBG_MSG("set pinmux i2c2 to gpio failed\n");
	close(i2c_file);
	return 0;

}
