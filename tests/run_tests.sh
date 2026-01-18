#!/bin/bash

set -o errexit

if [ ! -e c64-roms.h ]; then
  wget https://raw.githubusercontent.com/floooh/chips-test/refs/heads/master/examples/roms/c64-roms.h
  # patch: skip RAM check
  perl -i -pe 's/0xb3, 0xa8, 0xa9, 0x03, 0x85, 0xc2, 0xe6, 0xc2, 0xb1, 0xc1,/0xb3, 0xa8, 0xa9, 0x9f, 0x85, 0xc2, 0xe6, 0xc2, 0xb1, 0xc1,/gm' c64-roms.h
fi
if [ ! -e c1541-roms.h ]; then
  wget https://raw.githubusercontent.com/floooh/chips-test/refs/heads/master/examples/roms/c1541-roms.h
fi

gcc -o c64-ascii c64-ascii.c -lncurses

./c64-ascii $@
