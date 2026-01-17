#!/bin/bash

set -o errexit

if [ ! -e c64-roms.h ]; then
  wget https://raw.githubusercontent.com/floooh/chips-test/refs/heads/master/examples/roms/c64-roms.h
fi
if [ ! -e c1541-roms.h ]; then
  wget https://raw.githubusercontent.com/floooh/chips-test/refs/heads/master/examples/roms/c1541-roms.h
fi

gcc -o c64-ascii c64-ascii.c -lncurses

./c64-ascii
