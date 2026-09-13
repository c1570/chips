#!/bin/sh
# builds the w65c816 single-step test runner
set -e
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c99 -Wall -Wextra -O2 -o build/w65c816_test tests/w65c816_test.c
echo "build/w65c816_test built."
