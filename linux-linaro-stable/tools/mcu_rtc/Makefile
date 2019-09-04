CC = $(CROSS_COMPILE)gcc

SRCS = mcu_rtc.c
SRCS += devmem.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : efuse

efuse : $(OBJS)
	$(CC) -o mcu_rtc $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) mcu_rtc
