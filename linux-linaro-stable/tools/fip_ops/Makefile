CC = $(CROSS_COMPILE)gcc

SRCS = fip_ops.c
SRCS += devmem.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : fip_ops

fip_ops : $(OBJS)
	$(CC) -o fip_ops $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) fip_ops
