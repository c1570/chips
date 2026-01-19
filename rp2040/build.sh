#!/bin/bash
# Build script for RP2040 C1541 floppy drive emulator

set -o errexit

# Set PICO_SDK_PATH if not already set
if [ -z "$PICO_SDK_PATH" ]; then
    export PICO_SDK_PATH=$HOME/pico-sdk
fi

# Create build directory
mkdir -p build
cd build

# Run CMake configuration
cmake .. -DPICO_BOARD=pico

# Build the project
make -j$(nproc)

echo "Build complete!"
echo "Output files:"
ls -la c1541.* 2>/dev/null || echo "No output files found"
