#!/bin/bash
# Build + test the B-R cartridge demo
set -e
cd "$(dirname "$0")"

echo "=== Assembling cartridge ==="
acme -f plain -o cart-br-read.bin cart-br-read.asm 2>&1 | grep -vi warning || true
echo "cart-br-read.bin: $(wc -c < cart-br-read.bin) bytes"

echo ""
echo "=== Building emulator ==="
gcc -o c64-ascii c64-ascii.c -lncurses

echo ""
echo "=== Running autotest ==="
./c64-ascii --cart cart-br-read.bin -d ../docs/1541_test_demo.d64 --autotest
result=$?

echo ""
if [ $result -eq 0 ]; then
    echo "SUCCESS: B-R cartridge test passed"
else
    echo "FAILURE: B-R cartridge test failed (exit $result)"
fi
exit $result
