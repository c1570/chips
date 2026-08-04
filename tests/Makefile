# Makefile for the C64 emulation library and the C test runner

CC = gcc
CFLAGS = -std=c11 -Wall -fPIC -D_DEFAULT_SOURCE
INCLUDES = -I../chips -I../systems

TARGET = libc64_emulation.so
SOURCE = c64_emulation_wrapper.c

RUNNER = rp2_test_runner
RUNNER_CFLAGS = -std=c11 -Wall -O3 -D_DEFAULT_SOURCE

.PHONY: all clean

all: $(TARGET) $(RUNNER)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) -shared -o $(TARGET) $(SOURCE)

$(RUNNER): $(RUNNER).c rp2350js-c.h $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o c64_emulation_wrapper.o $(SOURCE)
	$(CC) $(RUNNER_CFLAGS) -o $(RUNNER) $(RUNNER).c c64_emulation_wrapper.o -lm

cart-br-read.bin: cart-br-read.asm
	acme -f plain -o cart-br-read.bin cart-br-read.asm

autotest: $(RUNNER).c rp2350js-c.h $(SOURCE) cart-br-read.bin
	$(CC) $(CFLAGS) $(INCLUDES) -DAUTOTEST -c -o c64_emulation_wrapper.o $(SOURCE)
	$(CC) $(RUNNER_CFLAGS) -DAUTOTEST -o rp2_autotest $(RUNNER).c c64_emulation_wrapper.o -lm

clean:
	rm -f $(TARGET) $(RUNNER) rp2_autotest c64_emulation_wrapper.o
