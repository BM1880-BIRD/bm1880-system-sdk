CC = $(CROSS_COMPILE)gcc

SRCS = libbm1880sn.c \
	devmem.c
OBJS = $(SRCS:.c=.o)
TARGET = libbm1880sn.so
CFLAGS = -fPIC -c

all : $(TARGET)

$(OBJS):
	$(CC) $(SRCS) $(CFLAGS)

$(TARGET) : $(OBJS)
	$(CC) -shared -Wl,-soname,$(TARGET) $(OBJS) -o $(TARGET)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) $(TARGET)
