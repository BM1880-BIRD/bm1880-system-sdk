CC = $(CROSS_COMPILE)gcc
PTHREAD_LIBS = -lpthread
SRCS = testboard.c
SRCS += uart.c


OBJS = $(SRCS:.c=.o)

LDFLAGS = $(PTHREAD_LIBS)

all : testboard

testboard : $(OBJS)
	$(CC) -o testboard $(SRCS)  $(LDFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) testboard
