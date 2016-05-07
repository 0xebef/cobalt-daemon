CC := gcc
RM := rm -f

CFLAGS ?= -O2
override CFLAGS += -D_FORTIFY_SOURCE=2 -std=gnu11 -pipe -pie -fpie -fno-plt -fexceptions -fasynchronous-unwind-tables -fno-strict-aliasing -fno-strict-overflow -fwrapv -static-libgcc -fvisibility=hidden -fstack-clash-protection -fstack-protector-strong -fcf-protection=full --param ssp-buffer-size=4 -Wall -Wextra -Werror -Wshadow -Wformat -Wformat-security

LDFLAGS ?= -Wl,-pie -Wl,-z,relro -Wl,-z,now -Wl,-z,defs -Wl,-s -Wl,-lrt

all: example

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

cobalt-daemon.o:

example.o:

example: cobalt-daemon.o example.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	$(RM) example *.o
