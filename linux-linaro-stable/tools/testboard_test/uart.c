#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<termios.h>
#include<errno.h>
#include<string.h>
#include "testboard.h"



int UART0_Open(int fd, char *port)
{
	fd = open(port, O_RDWR|O_NOCTTY);//   O_NDELAY);
	if (fd < 0) {
		perror("Can't Open Serial Port");
		return -1;
	}

	//set uart block
	if (fcntl(fd, F_SETFL, 0) < 0) {
		perror("fcntl failed!\n");
		return -1;
	}

	return fd;
}

void UART0_Close(int fd)
{
	close(fd);
}

/*
 * name:           UART0_Set
 * function:       set uart data bit , speed and so on
 * input args:     fd         file discriptor
 *                 speed      uart transmit speed
 *                 flow_ctrl  data flow contrl
 *                 databits   databits,should be set 7 or 8
 *                 stopbits   stop bits, should be set 1 or 2
 *                 parity     parity, should be set N,E,O,
 * return val:     success return 0，error return -1
 */
int UART0_Set(int fd, int speed,
			int flow_ctrl, int databits, int stopbits, int parity) {

	int   i;
	int   status;
	int   speed_arr[] = { B115200,
				B19200, B9600, B4800, B2400, B1200, B300};
	int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};
	struct termios options;

	//get the configuration args of the fd ,
	//it can test configuration correct or not too
	if (tcgetattr(fd, &options) != 0) {
		perror("SetupSerial 1");
		return -1;
	}

	for (i = 0; i < sizeof(speed_arr)/sizeof(int); i++) {
		if (speed == name_arr[i]) {
			cfsetispeed(&options, speed_arr[i]);
			cfsetospeed(&options, speed_arr[i]);
		}
	}

	//modify control mode ,keep thhis program woun't ocuppy the uart
	options.c_cflag |= CLOCAL;
	// enable receiver
	options.c_cflag |= CREAD;

	switch (flow_ctrl) {
	case 0://disable RTC/CTS(hardware) flow control
		options.c_cflag &= ~CRTSCTS;
		break;
	case 1://disable RTC/CTS(hardware) flow control
		options.c_cflag |= CRTSCTS;
		break;
	case 2://enable software flow control
		options.c_cflag |= IXON | IXOFF | IXANY;
		break;
	}

	//set bits per byte
	options.c_cflag &= ~CSIZE;
	switch (databits) {
	case 5:
		options.c_cflag |= CS5;
		break;
	case 6:
		options.c_cflag |= CS6;
		break;
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Unsupported data size\n");
		return -1;
	}

	switch (parity) {
	case 'n':
	case 'N': //disalbe parity
		options.c_cflag &= ~PARENB;
		options.c_iflag &= ~INPCK;
		break;
	case 'o':
	case 'O'://odd parity
		options.c_cflag |= (PARODD | PARENB);
		options.c_iflag |= INPCK;
		break;
	case 'e':
	case 'E'://even parity
		options.c_cflag |= PARENB;
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK;
		break;
	default:
		fprintf(stderr, "Unsupported parity\n");
		return -1;
	}

	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;  //1 stop bits
		break;
	case 2:
		options.c_cflag |= CSTOPB;  //2 stop bits
		break;
	default:
		fprintf(stderr, "Unsupported stop bits\n");
		return -1;
	}

	//raw output instead of processed output
	options.c_oflag &= ~OPOST;

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //ion canonical mode
	//options.c_lflag |= (ICANON | ECHO | ECHOE); //canonical mode
	//options.c_lflag &= ~(ECHO | ECHOE); //canonical mode

	options.c_cc[VTIME] = 1; /* 0.1s  */
	options.c_cc[VMIN] = 20;

	//tcflush() discards data written to the object referred to by fd
	//but not transmitted, or data received but not read,
	//depending on the value of queue_selector:
	tcflush(fd, TCIFLUSH);

	//active config
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		perror("com set error!\n");
		return -1;
	}
	return 0;
}


void *thread_fn(void *arg)
{
	int fd;
	int ret;
	int len, len1;
	int i = 0;
	char rcv_buf[4096];

	fd = UART0_Open(fd, arg);
	if (fd < 0) {
		printf("open %s failed,err=%d", (char *)arg, errno);
		return NULL;
	}
	ret = UART0_Set(fd, 115200, 0, 8, 1, 'N');
	if (ret < 0) {
		printf("uart opt set failed, err=%d", errno);
		return NULL;
	}
	printf("Set Porti %s Exactly!\n", (char *)arg);
	while (i++ < 100) {
		len = read(fd, rcv_buf, 4096);
		if (len > 0) {
			rcv_buf[len] = '\0';
			printf("receive data is %s\n", rcv_buf);
			len1 = write(fd, rcv_buf, len);
			if (len > 0 && len1 == len) {
				printf("send %d data successful\n", len);
			}
		} else {
			printf("cannot receive data\n");
		}
		sleep(2);
	}

	UART0_Close(fd);
	return arg;
}

