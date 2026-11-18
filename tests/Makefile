# Makefile for the C64 emulation library and the C test runner

CC = gcc
CFLAGS = -std=c11 -Wall -fPIC -D_DEFAULT_SOURCE
INCLUDES = -I../chips -I../systems

TARGET = libc64_emulation.so
SOURCE = c64_emulation_wrapper.c

RUNNER = rp2_test_runner
RUNNER_CFLAGS = -std=c11 -Wall -O3 -D_DEFAULT_SOURCE

.PHONY: all clean autotest dirtest-sd sdcard_model_test

all: $(TARGET) $(RUNNER)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) -shared -o $(TARGET) $(SOURCE)

SD_MODEL = sdcard_model.c sdcard_model.h

$(RUNNER): $(RUNNER).c rp2350js-c.h $(SOURCE) $(SD_MODEL)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o c64_emulation_wrapper.o $(SOURCE)
	$(CC) $(RUNNER_CFLAGS) -o $(RUNNER) $(RUNNER).c sdcard_model.c c64_emulation_wrapper.o -lm

cart-br-read.bin: cart-br-read.asm
	acme -f plain -o cart-br-read.bin cart-br-read.asm

autotest: $(RUNNER).c rp2350js-c.h $(SOURCE) $(SD_MODEL) cart-br-read.bin
	$(CC) $(CFLAGS) $(INCLUDES) -DAUTOTEST -c -o c64_emulation_wrapper.o $(SOURCE)
	$(CC) $(RUNNER_CFLAGS) -DAUTOTEST -o rp2_autotest $(RUNNER).c sdcard_model.c c64_emulation_wrapper.o -lm

# SD card model unit test (host only, fast)
sdcard_model_test: sdcard_model_test.c $(SD_MODEL)
	$(CC) -std=c11 -Wall -Wextra -O1 -g -fsanitize=address,undefined -o $@ sdcard_model_test.c sdcard_model.c
	./sdcard_model_test

# SD card images: FAT32 with a partition table, FAT16 without one
sd-fat32-mbr.img: make_sd_image.sh ../docs/1541_test_demo.d64
	./make_sd_image.sh $@ fat32 mbr

sd-fat16-sf.img: make_sd_image.sh ../docs/1541_test_demo.d64
	./make_sd_image.sh $@ fat16 superfloppy

# Directory listing of the SD card, checked on the C64 screen
rp2_dirtest: $(RUNNER).c rp2350js-c.h $(SOURCE) $(SD_MODEL)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o c64_emulation_wrapper.o $(SOURCE)
	$(CC) $(RUNNER_CFLAGS) -DDIRTEST -o rp2_dirtest $(RUNNER).c sdcard_model.c c64_emulation_wrapper.o -lm

# FAT root of both images, the D64 on them after CD:IMAGE.D64, and a failing CD
dirtest-sd: rp2_dirtest sd-fat32-mbr.img sd-fat16-sf.img
	SD2IEC_SDIMG=sd-fat32-mbr.img DIRTEST_FAT=32 ./rp2_dirtest
	SD2IEC_SDIMG=sd-fat16-sf.img DIRTEST_FAT=16 ./rp2_dirtest
	SD2IEC_SDIMG=sd-fat32-mbr.img SD2IEC_CD=IMAGE.D64 ./rp2_dirtest
	SD2IEC_SDIMG=sd-fat16-sf.img SD2IEC_CD=IMAGE.D64 ./rp2_dirtest
	# CD to a missing image must fail cleanly and leave the FAT root current
	SD2IEC_SDIMG=sd-fat32-mbr.img SD2IEC_CD=NOPE.D64 DIRTEST_EXPECT=root DIRTEST_FAT=32 ./rp2_dirtest

clean:
	rm -f $(TARGET) $(RUNNER) rp2_autotest rp2_dirtest sdcard_model_test c64_emulation_wrapper.o sd-*.img
