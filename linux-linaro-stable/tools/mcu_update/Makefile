CC = $(CROSS_COMPILE)gcc

SRCS = mcu_update.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : mcu_update

mcu_update : $(OBJS)
	$(CC) -o mcu_update $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) mcu_update
