CC = $(CROSS_COMPILE)gcc

SRCS = bm1880_get_sn.c
OBJS = $(SRCS:.c=.o)
TARGET = $(SRCS:.c=)

CFLAGS = -g

all : $(TARGET)

$(TARGET) : $(OBJS) lib/libbm1880sn.so
	$(CC) -o $(TARGET) $(SRCS) -lbm1880sn -Llib  $(CFLAGS)
	rm -f $(OBJS)

lib/libbm1880sn.so:
	make -C lib

.PHONY : clean
clean:
	rm -f $(OBJS) $(TARGET)
