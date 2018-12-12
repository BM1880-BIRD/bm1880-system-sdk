CC = $(CROSS_COMPILE)gcc

SRCS = dbg_i2c.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : dbg_i2c

efuse : $(OBJS)
	$(CC) -o dbg_i2c $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) dbg_i2c
