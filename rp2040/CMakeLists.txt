# RP2040 Commodore 1541 Floppy Drive Emulator
cmake_minimum_required(VERSION 3.13)

# Initialize the SDK directly from local path
include($ENV{HOME}/pico-sdk/pico_sdk_init.cmake)

project(c1541_rp2040 C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Add chips include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../chips)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../systems)

# Add the executable
add_executable(c1541
    c1541.c
)

# Link against required Pico SDK libraries
target_link_libraries(c1541
    pico_stdlib
    hardware_gpio
    hardware_structs
)

# Enable USB output, disable UART output (since we use GPIO for IEC)
pico_enable_stdio_usb(c1541 0)
pico_enable_stdio_uart(c1541 1)

# Create map/bin/hex/uf2 file in addition to ELF
pico_add_extra_outputs(c1541)
