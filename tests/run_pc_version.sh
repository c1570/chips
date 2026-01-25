#!/bin/bash

set -o errexit

. ./fetch_roms.sh

gcc -o c64-ascii c64-ascii.c -lncurses $BUILDPARMS

./c64-ascii $@
