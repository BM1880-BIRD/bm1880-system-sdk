#CROSS_COMPILE := aarch64-linux-gnu-

CC := $(CROSS_COMPILE)gcc

CFLAGS += -lm -g -DENV_EMU

go_test: *.c emu/*.c
	$(CC) -o $@ *.c emu/*.c $(CFLAGS)

clean:
	rm go_test
