# Makefile for C64 emulation library

CC = gcc
CFLAGS = -std=c11 -Wall -fPIC -D_DEFAULT_SOURCE
INCLUDES = -I../chips -I../systems

TARGET = libc64_emulation.so
SOURCE = c64_emulation_wrapper.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) -shared -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)
