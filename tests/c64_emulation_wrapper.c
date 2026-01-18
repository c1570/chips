/*
 * C64 Emulation Wrapper
 *
 * koffi-callable C64 emulator library for RP2040 C1541 testing.
 * Uses the same IEC bus pattern as rp2040/c1541.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define CHIPS_IMPL
#include "../chips/chips_common.h"
#include "../chips/m6502.h"
#include "../chips/m6522.h"
#include "../chips/m6526.h"
#include "../chips/m6569.h"
#include "../chips/m6581.h"
#include "../chips/mem.h"
#include "../chips/clk.h"
#include "../chips/kbd.h"
#include "../systems/c1530.h"
#include "../systems/c1541.h"
#include "../systems/c64.h"
#include "../systems/iecbus.h"
#include "c64-roms.h"
#include "c1541-roms.h"

// Global C64 state
static c64_t c64;
static bool initialized = false;
static iecbus_device_t* host_iec = NULL;  // C64's IEC device
static uint64_t c64_tick_count = 0;        // Counter for C64 ticks

// Initialize C64 WITHOUT C1541 emulation
void c64_emulation_init() {
    c64_init(&c64, &(c64_desc_t){
        .roms = {
            .chars = { .ptr=dump_c64_char_bin, .size=sizeof(dump_c64_char_bin) },
            .basic = { .ptr=dump_c64_basic_bin, .size=sizeof(dump_c64_basic_bin) },
            .kernal = { .ptr=dump_c64_kernalv3_bin, .size=sizeof(dump_c64_kernalv3_bin) }
            // NO c1541 roms - we're using RP2040 instead
        },
        .c1541_enabled = 0  // C1541 DISABLED - using RP2040 instead
    });

    // Connect C64 as an IEC device (same pattern as rp2040/c1541.c)
    host_iec = iec_connect(&c64.iec_bus);
    initialized = true;
}

void set_keybuf(char* str) {
    c64.ram[198] = strlen(str);
    for(uint i=0; i<c64.ram[198]; i++) { c64.ram[631+i]=str[i]; };
}

// Tick C64 emulation (called on STROBE rising edge from RP2040)
// This is a stopgap for proper timing - C64 runs when RP2040 signals it
void c64_emulation_tick() {
    if (!initialized) return;

    // Tick the C64 for a small amount of time
    // This gets called frequently by the RP2040's STROBE pin
    c64_tick_count += c64_exec(&c64, 2);  // Execute for 1 tick (2µS, rounded down)

    if(c64_tick_count == 150000) {
      set_keybuf("L\x6f\"$\",8\r");
    }
}

// Get current C64 video buffer pointers and colors (for display)
void c64_get_video_buffer(const uint8_t** fb, const uint8_t** color_ram,
                         uint8_t* bg_color, uint8_t* border_color) {
    *fb = c64.fb;                           // VIC framebuffer
    *color_ram = c64.color_ram;              // Color RAM (1024 bytes)
    *bg_color = c64.vic.gunit.bg[0] & 0xF;    // Background color
    *border_color = c64.vic.brd.bc & 0xF;     // Border color
}

// Send keypress to C64 (wrapper that calls the actual c64 function)
void c64_key_down_wrapper(int key_code) {
    c64_key_down(&c64, key_code);
}

void c64_key_up_wrapper(int key_code) {
    c64_key_up(&c64, key_code);
}

// IEC GPIO Interface - Called by RP2040 GPIO changes
// Uses the SAME pattern as rp2040/c1541.c
// gpio_state: bit 0=DATA, 1=CLK, 2=ATN, 4=RESET (active-low, same as IECLINE_* bits)
void c64_set_iec_gpio(uint8_t gpio_state) {
    // Directly set C64's IEC device signals from GPIO state
    // gpio_state format matches IECLINE_* bits exactly
    iec_set_signals(c64.iec_bus, host_iec, gpio_state);
}

// Get combined IEC bus state (C64 + RP2040 + any other devices)
// Returns state in IECLINE_* bit format
uint8_t c64_get_iec_bus() {
    return iec_get_signals(c64.iec_bus);
}

// Get the C64 tick count
uint64_t c64_get_tick_count() {
    return c64_tick_count;
}

// Print C64 tick count (debugging)
void c64_print_tick_count() {
    printf("C64 ticks: %lu\r", (unsigned long)c64_tick_count);
    fflush(stdout);
}

// Conversion table from C64 font index to ASCII (the 'x' is actually the pound sign)
static const char font_map[65] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[x]   !\"#$%&`()*+,-./0123456789:;<=>?";

// Get text screen buffer (40x25 characters)
// Fills output buffers with screen content
// chars_out: 1000 bytes (40x25) - ASCII characters
// colors_out: 1000 bytes (40x25) - color indices (0-15)
// Returns: background color in lower 4 bits, border color in upper 4 bits
uint8_t c64_get_text_screen(uint8_t* chars_out, uint8_t* colors_out) {
    if (!initialized) return 0;

    // Screen memory is at 0x0400 in C64 memory map (VIC sees it there)
    const uint16_t screen_base = 0x0400;

    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 40; x++) {
            int offset = y * 40 + x;

            // Get character code from screen memory
            uint8_t font_code = mem_rd(&c64.mem_vic, screen_base + offset);

            // Convert to ASCII using font_map
            char chr = font_map[font_code & 63];
            chars_out[offset] = chr;

            // Get color from color RAM (lower 4 bits)
            colors_out[offset] = c64.color_ram[offset] & 15;
        }
    }

    // Return background color (lower 4 bits) and border color (upper 4 bits)
    uint8_t bg = c64.vic.gunit.bg[0] & 0xF;
    uint8_t border = c64.vic.brd.bc & 0xF;
    return (border << 4) | bg;
}

// Print C64 screen to stdout (using ANSI escape codes for colors)
void c64_print_screen() {
    if (!initialized) return;

    const uint16_t screen_base = 0x0400;

    // Clear screen and move cursor to top-left
    printf("\033[2J\033[H");

    // Print 25 rows of 40 characters each
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 40; x++) {
            int offset = y * 40 + x;

            // Get character code from screen memory
            uint8_t font_code = mem_rd(&c64.mem_vic, screen_base + offset);

            // Convert to ASCII using font_map
            char chr = font_map[font_code & 63];

            // Get color and check for reverse video
            uint8_t fg = c64.color_ram[offset] & 15;
            int reverse = (font_code > 127);

            // Print character with ANSI color
            // Using simple ANSI colors (not the full xterm-256color palette)
            if (reverse) {
                printf("\033[7m%c\033[0m", chr);  // reverse video
            } else {
                printf("%c", chr);
            }
        }
        printf("\n");
    }

    // Print status line at bottom
    uint8_t bg = c64.vic.gunit.bg[0] & 0xF;
    uint8_t border = c64.vic.brd.bc & 0xF;
    printf("\nC64 ticks: %lu | BG: %d | Border: %d\n",
           (unsigned long)c64_tick_count, bg, border);
    fflush(stdout);
}
