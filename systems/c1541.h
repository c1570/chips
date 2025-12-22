#pragma once
/*#
    # c1551.h

    A Commodore 1541 floppy drive emulation.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    You need to include the following headers before including c64.h:

    - chips/chips_common.h
    - chips/m6502.h
    - chips/m6522.h
    - chips/mem.h

    ## zlib/libpng license

    Copyright (c) 2019 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "iecbus.h"
#include "disass.h"
#include "gcr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C1541_FREQUENCY (1000000)

#define RUNTIME_LIMIT_TICKS 1000

#define VIA2_STEPPER_LO_BIT_POS  0
#define VIA2_STEPPER_HI_BIT_POS  1
#define VIA2_ROTOR_BIT_POS       2
#define VIA2_LED_BIT_POS         3
#define VIA2_READ_ONLY_BIT_POS   4
#define VIA2_BIT_RATE_LO_BIT_POS 5
#define VIA2_BIT_RATE_HI_BIT_POS 6
#define VIA2_SYNC_BIT_POS        7

#define VIA2_STEPPER_LO          (1<<VIA2_STEPPER_LO_BIT_POS)
#define VIA2_STEPPER_HI          (1<<VIA2_STEPPER_HI_BIT_POS)
#define VIA2_ROTOR               (1<<VIA2_ROTOR_BIT_POS)
#define VIA2_LED                 (1<<VIA2_LED_BIT_POS)
#define VIA2_READ_ONLY           (1<<VIA2_READ_ONLY_BIT_POS)
#define VIA2_BIT_RATE_LO         (1<<VIA2_BIT_RATE_LO_BIT_POS)
#define VIA2_BIT_RATE_HI         (1<<VIA2_BIT_RATE_HI_BIT_POS)
#define VIA2_SYNC                (1<<VIA2_SYNC_BIT_POS)


// config params for c1541_init()
typedef struct {
    // the IEC bus to connect to
    iecbus_t* iec_bus;
    // rom images
    struct {
        chips_range_t c000_dfff;
        chips_range_t e000_ffff;
    } roms;

#ifdef __IEC_DEBUG
    FILE* debug_file;
#endif
} c1541_desc_t;

// 1541 emulator state
typedef struct {
    uint64_t pins;
    iecbus_t* iec_bus;
    iecbus_device_t* iec_device;
    m6502_t cpu;
    m6522_t via_1;
    m6522_t via_2;
    bool valid;
    mem_t mem;
    uint8_t ram[0x0800];
    uint8_t rom[0x4000];
#ifdef __IEC_DEBUG
    FILE* debug_file;
#endif
    uint16_t hundred_cycles_rotor_active;
    uint16_t hundred_cycles_per_bit;
    bool rotor_active;
    uint8_t gcr_bytes[0x2000];
    uint16_t gcr_size;
    uint16_t gcr_byte_pos;
    uint8_t gcr_bit_pos;
    uint8_t gcr_ones;
    uint8_t current_byte;
    uint8_t current_bit_pos;
    bool gcr_sync;

    uint16_t current_data;
    uint8_t output_data;
    uint8_t output_bit_counter;
    bool byte_ready;
    uint32_t exit_countdown;
    uint8_t half_track;

    uint8_t *gcr_disk_data;
} c1541_t;

// initialize a new c1541_t instance
void c1541_init(c1541_t* sys, const c1541_desc_t* desc);
// discard a c1541_t instance
void c1541_discard(c1541_t* sys);
// reset a c1541_t instance
void c1541_reset(c1541_t* sys);
// tick a c1541_t instance forward
void c1541_tick(c1541_t* sys);
// insert a disc image file (.d64)
void c1541_insert_disc(c1541_t* sys, chips_range_t data);
// remove current disc
void c1541_remove_disc(c1541_t* sys);
// prepare a c1541_t snapshot for saving
void c1541_snapshot_onsave(c1541_t* snapshot, void* base);
// prepare a c1541_t snapshot for loading
void c1541_snapshot_onload(c1541_t* snapshot, c1541_t* sys, void* base);

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

//#include "../docs/1541_test_demo_track18gcr.h"
#include "../docs/1541_test_demo.h"

void c1541_init(c1541_t* sys, const c1541_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);

    uint8_t initial_full_track = 18;

    memset(sys, 0, sizeof(c1541_t));
#ifdef __IEC_DEBUG
    sys->debug_file = desc->debug_file;
#endif
    sys->valid = true;
    sys->gcr_disk_data = gcr_1541_test_demo_g64;
    sys->gcr_size = gcr_1541_test_demo_g64_len;
    gcr_get_half_track_bytes(sys->gcr_bytes, sys->gcr_disk_data, gcr_full_track_to_half_track(initial_full_track));
    sys->gcr_byte_pos = 0;
    sys->gcr_bit_pos = 0;
    sys->current_byte = 0;
    sys->current_bit_pos = 0;
    sys->hundred_cycles_per_bit = 369;
    sys->hundred_cycles_rotor_active = 0;
    sys->half_track = gcr_full_track_to_half_track(initial_full_track);

    // copy ROM images
    CHIPS_ASSERT(desc->roms.c000_dfff.ptr && (0x2000 == desc->roms.c000_dfff.size));
    CHIPS_ASSERT(desc->roms.e000_ffff.ptr && (0x2000 == desc->roms.e000_ffff.size));
    memcpy(&sys->rom[0x0000], desc->roms.c000_dfff.ptr, 0x2000);
    memcpy(&sys->rom[0x2000], desc->roms.e000_ffff.ptr, 0x2000);

    // initialize the hardware
    m6502_desc_t cpu_desc;
    memset(&cpu_desc, 0, sizeof(cpu_desc));
    sys->pins = m6502_init(&sys->cpu, &cpu_desc);
    m6522_init(&sys->via_1);
    m6522_init(&sys->via_2);

    sys->via_1.chip_name = "via1";
    sys->via_2.chip_name = "via2";

    // setup memory map
    mem_init(&sys->mem);
    mem_map_ram(&sys->mem, 0, 0x0000, 0x0800, sys->ram);
    mem_map_rom(&sys->mem, 0, 0xC000, 0x4000, sys->rom);

    sys->iec_bus = desc->iec_bus;
    CHIPS_ASSERT(sys->iec_bus);

    // Connect to IEC bus and tell it that we have inverter logic in our hardware
    sys->iec_device = iec_connect(sys->iec_bus, true);
    CHIPS_ASSERT(sys->iec_device);
}

void c1541_discard(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    iec_disconnect(sys->iec_bus, sys->iec_device);
    sys->iec_device = NULL;
    sys->valid = false;
}

void c1541_reset(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->pins |= M6502_RES;
    m6522_reset(&sys->via_1);
    m6522_reset(&sys->via_2);
}

static uint16_t _1541_last_cpu_address = 0;
uint8_t last_via2_0_write = 0xff;
uint8_t last_via2_1_write = 0xff;
uint8_t last_via2_2_write = 0xff;
uint8_t last_via2_3_write = 0xff;
void _c1541_write(c1541_t* sys, uint16_t addr, uint8_t data) {
    char *area = "n/a";
    bool illegal = false;
    bool changed = false;

    if ((addr & 0xFC00) == 0x1800) {
        area = "via1";
        _m6522_write(&sys->via_1, addr & 0xF, data);
    } else if ((addr & 0xFC00) == 0x1C00) {
        // Write to VIA2
        area = "via2";
        if (addr == 0x1C00) {
            sys->rotor_active = (data & VIA2_ROTOR) != 0;
            if (!sys->rotor_active) {
                sys->hundred_cycles_rotor_active = 0;
            }
	    // FIXME: use correct formula once the cpu frequency is decoupled from c64 and uses real 1MHz
            // sys->hundred_cycles_per_bit = 400 - ((data & 3) * 25);
            switch (data & 3) {
                case 0:
                    sys->hundred_cycles_per_bit = 394;
                    break;
                case 1:
                    sys->hundred_cycles_per_bit = 369;
                    break;
                case 2:
                    sys->hundred_cycles_per_bit = 345;
                    break;
                case 3:
                    sys->hundred_cycles_per_bit = 320;
                    break;
            }
        }
        if ((addr & 0x0f) == 0 && (data != last_via2_0_write)) {
            last_via2_0_write = data;
            changed = true;
        }
        if ((addr & 0x0f) == 1 && (data != last_via2_1_write)) {
            last_via2_1_write = data;
            changed = true;
        }
        if ((addr & 0x0f) == 2 && (data != last_via2_2_write)) {
            last_via2_2_write = data;
            changed = true;
        }
        if ((addr & 0x0f) == 3 && (data != last_via2_3_write)) {
            last_via2_3_write = data;
            changed = true;
        }
	// FIXME: for debugging purpose only
        if (changed) {
            printf("%ld - 1541 - VIA2 Write $%04X = $%02X - CPU @ $%04X - Stepper=(%d%d[%d%d]) Rotor=(%d[%d]) LED=(%d[%d]) R/O=(%d[%d]) BitRate=(%d%d[%d%d]) Sync=(%d[%d])\n",
                get_world_tick(),
                addr,
                data,
                _1541_last_cpu_address,
                last_via2_0_write & (1<<1) ? 1 : 0,
                last_via2_0_write & (1<<0) ? 1 : 0,
                last_via2_2_write & (1<<1) ? 1 : 0,
                last_via2_2_write & (1<<0) ? 1 : 0,
                last_via2_0_write & (1<<2) ? 1 : 0,
                last_via2_2_write & (1<<2) ? 1 : 0,
                last_via2_0_write & (1<<3) ? 1 : 0,
                last_via2_2_write & (1<<3) ? 1 : 0,
                last_via2_0_write & (1<<4) ? 1 : 0,
                last_via2_2_write & (1<<4) ? 1 : 0,
                last_via2_0_write & (1<<6) ? 1 : 0,
                last_via2_0_write & (1<<5) ? 1 : 0,
                last_via2_2_write & (1<<6) ? 1 : 0,
                last_via2_2_write & (1<<5) ? 1 : 0,
                last_via2_0_write & (1<<7) ? 1 : 0,
                last_via2_2_write & (1<<7) ? 1 : 0
            );
        }
        _m6522_write(&sys->via_2, addr & 0xF, data);
    } else if (addr < 0x0800) {
        // Write to RAM
        area = "ram";
        sys->ram[addr & 0x7FF] = data;
    } else {
        // Illegal access
        area = "illegal";
        illegal = true;
    }
    if (illegal) {
        printf("1541-write %s $%x $%x\n", area, addr, data);
    }
}

#define XOR(a,b) (((a) && !(b)) || ((b) && !(a)))
#define AND(a,b) ((a) && (b))

void _c1541_write_iec_pins(c1541_t* sys, uint64_t pins) {
    uint8_t out_signals = 0;

    uint8_t iec_signals = iec_get_signals(sys->iec_bus, sys->iec_device);

    if ((pins & M6522_PB1) || (XOR(iec_signals & IECLINE_ATN, pins & M6522_PB4))) {
        // ATNA logic (UD3)
        out_signals |= IECLINE_DATA;
    }

    if ((pins & M6522_PB3)) {
        out_signals |= IECLINE_CLK;
    }

    if (out_signals != sys->iec_device->signals) {
        char message_prefix[256];
        sprintf(message_prefix, "%ld - 1541 - write-iec - CPU @ $%04X", get_world_tick(), _1541_last_cpu_address);
        sys->iec_device->signals = out_signals;
	// FIXME: for debugging purpose only
// #ifdef __IEC_DEBUG
        iec_debug_print_device_signals(sys->iec_device, message_prefix);
// #endif
    }
}

uint16_t _get_c1541_nearest_routine_address(uint16_t addr) {
    uint16_t res = 0;
    if (addr >= 0xFF0D) res = 0xFF0D;
    else if(addr >= 0xFF01) res = 0xFF01;
    else if(addr >= 0xFEF3) res = 0xFEF3;
    else if(addr >= 0xFEEA) res = 0xFEEA;
    else if(addr >= 0xFEE7) res = 0xFEE7;
    else if(addr >= 0xFE7F) res = 0xFE7F;
    else if(addr >= 0xFE76) res = 0xFE76;
    else if(addr >= 0xFE67) res = 0xFE67;
    else if(addr >= 0xFE44) res = 0xFE44;
    else if(addr >= 0xFE30) res = 0xFE30;
    else if(addr >= 0xFE26) res = 0xFE26;
    else if(addr >= 0xFE0E) res = 0xFE0E;
    else if(addr >= 0xFE00) res = 0xFE00;
    else if(addr >= 0xFDF5) res = 0xFDF5;
    else if(addr >= 0xFDE5) res = 0xFDE5;
    else if(addr >= 0xFDDB) res = 0xFDDB;
    else if(addr >= 0xFDD3) res = 0xFDD3;
    else if(addr >= 0xFDC9) res = 0xFDC9;
    else if(addr >= 0xFDC3) res = 0xFDC3;
    else if(addr >= 0xFDB9) res = 0xFDB9;
    else if(addr >= 0xFDA3) res = 0xFDA3;
    else if(addr >= 0xFD96) res = 0xFD96;
    else if(addr >= 0xFD77) res = 0xFD77;
    else if(addr >= 0xFD67) res = 0xFD67;
    else if(addr >= 0xFD62) res = 0xFD62;
    else if(addr >= 0xFD58) res = 0xFD58;
    else if(addr >= 0xFD40) res = 0xFD40;
    else if(addr >= 0xFD39) res = 0xFD39;
    else if(addr >= 0xFD2C) res = 0xFD2C;
    else if(addr >= 0xFD09) res = 0xFD09;
    else if(addr >= 0xFCF9) res = 0xFCF9;
    else if(addr >= 0xFCEB) res = 0xFCEB;
    else if(addr >= 0xFCE0) res = 0xFCE0;
    else if(addr >= 0xFCD1) res = 0xFCD1;
    else if(addr >= 0xFCC2) res = 0xFCC2;
    else if(addr >= 0xFCB8) res = 0xFCB8;
    else if(addr >= 0xFCB1) res = 0xFCB1;
    else if(addr >= 0xFC86) res = 0xFC86;
    else if(addr >= 0xFC3F) res = 0xFC3F;
    else if(addr >= 0xFBE0) res = 0xFBE0;
    else if(addr >= 0xFBCE) res = 0xFBCE;
    else if(addr >= 0xFBBB) res = 0xFBBB;
    else if(addr >= 0xFBB6) res = 0xFBB6;
    else if(addr >= 0xFB7D) res = 0xFB7D;
    else if(addr >= 0xFB67) res = 0xFB67;
    else if(addr >= 0xFB64) res = 0xFB64;
    else if(addr >= 0xFB5C) res = 0xFB5C;
    else if(addr >= 0xFB46) res = 0xFB46;
    else if(addr >= 0xFB43) res = 0xFB43;
    else if(addr >= 0xFB3E) res = 0xFB3E;
    else if(addr >= 0xFB39) res = 0xFB39;
    else if(addr >= 0xFB0C) res = 0xFB0C;
    else if(addr >= 0xFB00) res = 0xFB00;
    else if(addr >= 0xFAF5) res = 0xFAF5;
    else if(addr >= 0xFAC7) res = 0xFAC7;
    else if(addr >= 0xFABE) res = 0xFABE;
    else if(addr >= 0xFAA5) res = 0xFAA5;
    else if(addr >= 0xFA97) res = 0xFA97;
    else if(addr >= 0xFA94) res = 0xFA94;
    else if(addr >= 0xFA7B) res = 0xFA7B;
    else if(addr >= 0xFA69) res = 0xFA69;
    else if(addr >= 0xFA63) res = 0xFA63;
    else if(addr >= 0xFA4E) res = 0xFA4E;
    else if(addr >= 0xFA3B) res = 0xFA3B;
    else if(addr >= 0xFA32) res = 0xFA32;
    else if(addr >= 0xFA2E) res = 0xFA2E;
    else if(addr >= 0xFA1C) res = 0xFA1C;
    else if(addr >= 0xFA0E) res = 0xFA0E;
    else if(addr >= 0xFA05) res = 0xFA05;
    else if(addr >= 0xF9FA) res = 0xF9FA;
    else if(addr >= 0xF9E4) res = 0xF9E4;
    else if(addr >= 0xF9D9) res = 0xF9D9;
    else if(addr >= 0xF9D6) res = 0xF9D6;
    else if(addr >= 0xF99C) res = 0xF99C;
    else if(addr >= 0xF98F) res = 0xF98F;
    else if(addr >= 0xF97E) res = 0xF97E;
    else if(addr >= 0xF975) res = 0xF975;
    else if(addr >= 0xF969) res = 0xF969;
    else if(addr >= 0xF934) res = 0xF934;
    else if(addr >= 0xF92B) res = 0xF92B;
    else if(addr >= 0xF90C) res = 0xF90C;
    else if(addr >= 0xF8E0) res = 0xF8E0;
    else if(addr >= 0xF7E6) res = 0xF7E6;
    else if(addr >= 0xF7BA) res = 0xF7BA;
    else if(addr >= 0xF78F) res = 0xF78F;
    else if(addr >= 0xF77F) res = 0xF77F;
    else if(addr >= 0xF6D0) res = 0xF6D0;
    else if(addr >= 0xF6CA) res = 0xF6CA;
    else if(addr >= 0xF6C5) res = 0xF6C5;
    else if(addr >= 0xF6B3) res = 0xF6B3;
    else if(addr >= 0xF6A3) res = 0xF6A3;
    else if(addr >= 0xF691) res = 0xF691;
    else if(addr >= 0xF683) res = 0xF683;
    else if(addr >= 0xF66E) res = 0xF66E;
    else if(addr >= 0xF64F) res = 0xF64F;
    else if(addr >= 0xF643) res = 0xF643;
    else if(addr >= 0xF624) res = 0xF624;
    else if(addr >= 0xF5F2) res = 0xF5F2;
    else if(addr >= 0xF5E9) res = 0xF5E9;
    else if(addr >= 0xF5BF) res = 0xF5BF;
    else if(addr >= 0xF5B3) res = 0xF5B3;
    else if(addr >= 0xF5AB) res = 0xF5AB;
    else if(addr >= 0xF586) res = 0xF586;
    else if(addr >= 0xF56E) res = 0xF56E;
    else if(addr >= 0xF55D) res = 0xF55D;
    else if(addr >= 0xF556) res = 0xF556;
    else if(addr >= 0xF553) res = 0xF553;
    else if(addr >= 0xF54E) res = 0xF54E;
    else if(addr >= 0xF53D) res = 0xF53D;
    else if(addr >= 0xF538) res = 0xF538;
    else if(addr >= 0xF510) res = 0xF510;
    else if(addr >= 0xF50A) res = 0xF50A;
    else if(addr >= 0xF4FB) res = 0xF4FB;
    else if(addr >= 0xF4DF) res = 0xF4DF;
    else if(addr >= 0xF4D4) res = 0xF4D4;
    else if(addr >= 0xF4D1) res = 0xF4D1;
    else if(addr >= 0xF4CA) res = 0xF4CA;
    else if(addr >= 0xF497) res = 0xF497;
    else if(addr >= 0xF483) res = 0xF483;
    else if(addr >= 0xF47E) res = 0xF47E;
    else if(addr >= 0xF473) res = 0xF473;
    else if(addr >= 0xF461) res = 0xF461;
    else if(addr >= 0xF43A) res = 0xF43A;
    else if(addr >= 0xF432) res = 0xF432;
    else if(addr >= 0xF423) res = 0xF423;
    else if(addr >= 0xF41E) res = 0xF41E;
    else if(addr >= 0xF41B) res = 0xF41B;
    else if(addr >= 0xF418) res = 0xF418;
    else if(addr >= 0xF410) res = 0xF410;
    else if(addr >= 0xF407) res = 0xF407;
    else if(addr >= 0xF3D8) res = 0xF3D8;
    else if(addr >= 0xF3C8) res = 0xF3C8;
    else if(addr >= 0xF3B1) res = 0xF3B1;
    else if(addr >= 0xF393) res = 0xF393;
    else if(addr >= 0xF37C) res = 0xF37C;
    else if(addr >= 0xF377) res = 0xF377;
    else if(addr >= 0xF367) res = 0xF367;
    else if(addr >= 0xF33C) res = 0xF33C;
    else if(addr >= 0xF320) res = 0xF320;
    else if(addr >= 0xF306) res = 0xF306;
    else if(addr >= 0xF2F9) res = 0xF2F9;
    else if(addr >= 0xF2F3) res = 0xF2F3;
    else if(addr >= 0xF2E9) res = 0xF2E9;
    else if(addr >= 0xF2D8) res = 0xF2D8;
    else if(addr >= 0xF2CD) res = 0xF2CD;
    else if(addr >= 0xF2C3) res = 0xF2C3;
    else if(addr >= 0xF2BE) res = 0xF2BE;
    else if(addr >= 0xF2B0) res = 0xF2B0;
    else if(addr >= 0xF259) res = 0xF259;
    else if(addr >= 0xF258) res = 0xF258;
    else if(addr >= 0xF24E) res = 0xF24E;
    else if(addr >= 0xF24B) res = 0xF24B;
    else if(addr >= 0xF246) res = 0xF246;
    else if(addr >= 0xF236) res = 0xF236;
    else if(addr >= 0xF22D) res = 0xF22D;
    else if(addr >= 0xF22B) res = 0xF22B;
    else if(addr >= 0xF220) res = 0xF220;
    else if(addr >= 0xF21F) res = 0xF21F;
    else if(addr >= 0xF21D) res = 0xF21D;
    else if(addr >= 0xF20D) res = 0xF20D;
    else if(addr >= 0xF1FA) res = 0xF1FA;
    else if(addr >= 0xF1F5) res = 0xF1F5;
    else if(addr >= 0xF1E6) res = 0xF1E6;
    else if(addr >= 0xF1DF) res = 0xF1DF;
    else if(addr >= 0xF1CB) res = 0xF1CB;
    else if(addr >= 0xF1A9) res = 0xF1A9;
    else if(addr >= 0xF19D) res = 0xF19D;
    else if(addr >= 0xF19A) res = 0xF19A;
    else if(addr >= 0xF195) res = 0xF195;
    else if(addr >= 0xF173) res = 0xF173;
    else if(addr >= 0xF15F) res = 0xF15F;
    else if(addr >= 0xF15A) res = 0xF15A;
    else if(addr >= 0xF130) res = 0xF130;
    else if(addr >= 0xF12D) res = 0xF12D;
    else if(addr >= 0xF11E) res = 0xF11E;
    else if(addr >= 0xF119) res = 0xF119;
    else if(addr >= 0xF118) res = 0xF118;
    else if(addr >= 0xF10F) res = 0xF10F;
    else if(addr >= 0xF10A) res = 0xF10A;
    else if(addr >= 0xF0F2) res = 0xF0F2;
    else if(addr >= 0xF0DF) res = 0xF0DF;
    else if(addr >= 0xF0D1) res = 0xF0D1;
    else if(addr >= 0xF0D0) res = 0xF0D0;
    else if(addr >= 0xF0BE) res = 0xF0BE;
    else if(addr >= 0xF0A5) res = 0xF0A5;
    else if(addr >= 0xF09F) res = 0xF09F;
    else if(addr >= 0xF07F) res = 0xF07F;
    else if(addr >= 0xF05B) res = 0xF05B;
    else if(addr >= 0xF03E) res = 0xF03E;
    else if(addr >= 0xF022) res = 0xF022;
    else if(addr >= 0xF011) res = 0xF011;
    else if(addr >= 0xF00B) res = 0xF00B;
    else if(addr >= 0xF005) res = 0xF005;
    else if(addr >= 0xF004) res = 0xF004;
    else if(addr >= 0xEFF1) res = 0xEFF1;
    else if(addr >= 0xEFE9) res = 0xEFE9;
    else if(addr >= 0xEFD5) res = 0xEFD5;
    else if(addr >= 0xEFD3) res = 0xEFD3;
    else if(addr >= 0xEFCF) res = 0xEFCF;
    else if(addr >= 0xEFCE) res = 0xEFCE;
    else if(addr >= 0xEFBD) res = 0xEFBD;
    else if(addr >= 0xEFBA) res = 0xEFBA;
    else if(addr >= 0xEF93) res = 0xEF93;
    else if(addr >= 0xEF90) res = 0xEF90;
    else if(addr >= 0xEF88) res = 0xEF88;
    else if(addr >= 0xEF87) res = 0xEF87;
    else if(addr >= 0xEF62) res = 0xEF62;
    else if(addr >= 0xEF5F) res = 0xEF5F;
    else if(addr >= 0xEF5C) res = 0xEF5C;
    else if(addr >= 0xEF4D) res = 0xEF4D;
    else if(addr >= 0xEF3A) res = 0xEF3A;
    else if(addr >= 0xEF24) res = 0xEF24;
    else if(addr >= 0xEF07) res = 0xEF07;
    else if(addr >= 0xEEFF) res = 0xEEFF;
    else if(addr >= 0xEEF4) res = 0xEEF4;
    else if(addr >= 0xEEE3) res = 0xEEE3;
    else if(addr >= 0xEED9) res = 0xEED9;
    else if(addr >= 0xEEC7) res = 0xEEC7;
    else if(addr >= 0xEEB7) res = 0xEEB7;
    else if(addr >= 0xEE56) res = 0xEE56;
    else if(addr >= 0xEE46) res = 0xEE46;
    else if(addr >= 0xEE19) res = 0xEE19;
    else if(addr >= 0xEE0D) res = 0xEE0D;
    else if(addr >= 0xEE04) res = 0xEE04;
    else if(addr >= 0xEDEE) res = 0xEDEE;
    else if(addr >= 0xEDE5) res = 0xEDE5;
    else if(addr >= 0xEDD9) res = 0xEDD9;
    else if(addr >= 0xEDD4) res = 0xEDD4;
    else if(addr >= 0xEDCB) res = 0xEDCB;
    else if(addr >= 0xEDB3) res = 0xEDB3;
    else if(addr >= 0xED9C) res = 0xED9C;
    else if(addr >= 0xED84) res = 0xED84;
    else if(addr >= 0xED7E) res = 0xED7E;
    else if(addr >= 0xED6D) res = 0xED6D;
    else if(addr >= 0xED67) res = 0xED67;
    else if(addr >= 0xED5B) res = 0xED5B;
    else if(addr >= 0xED59) res = 0xED59;
    else if(addr >= 0xED23) res = 0xED23;
    else if(addr >= 0xED0D) res = 0xED0D;
    else if(addr >= 0xECEA) res = 0xECEA;
    else if(addr >= 0xEC9E) res = 0xEC9E;
    else if(addr >= 0xEC98) res = 0xEC98;
    else if(addr >= 0xEC8B) res = 0xEC8B;
    else if(addr >= 0xEC81) res = 0xEC81;
    else if(addr >= 0xEC6D) res = 0xEC6D;
    else if(addr >= 0xEC69) res = 0xEC69;
    else if(addr >= 0xEC5C) res = 0xEC5C;
    else if(addr >= 0xEC58) res = 0xEC58;
    else if(addr >= 0xEC3B) res = 0xEC3B;
    else if(addr >= 0xEC31) res = 0xEC31;
    else if(addr >= 0xEC2B) res = 0xEC2B;
    else if(addr >= 0xEC12) res = 0xEC12;
    else if(addr >= 0xEC07) res = 0xEC07;
    else if(addr >= 0xEBFF) res = 0xEBFF;
    else if(addr >= 0xEBE7) res = 0xEBE7;
    else if(addr >= 0xEBD5) res = 0xEBD5;
    else if(addr >= 0xEB7E) res = 0xEB7E;
    else if(addr >= 0xEB76) res = 0xEB76;
    else if(addr >= 0xEB4F) res = 0xEB4F;
    else if(addr >= 0xEB4B) res = 0xEB4B;
    else if(addr >= 0xEB22) res = 0xEB22;
    else if(addr >= 0xEB1F) res = 0xEB1F;
    else if(addr >= 0xEB04) res = 0xEB04;
    else if(addr >= 0xEB02) res = 0xEB02;
    else if(addr >= 0xEAF2) res = 0xEAF2;
    else if(addr >= 0xEAF0) res = 0xEAF0;
    else if(addr >= 0xEAEC) res = 0xEAEC;
    else if(addr >= 0xEAEA) res = 0xEAEA;
    else if(addr >= 0xEAD7) res = 0xEAD7;
    else if(addr >= 0xEAD5) res = 0xEAD5;
    else if(addr >= 0xEAC9) res = 0xEAC9;
    else if(addr >= 0xEAB7) res = 0xEAB7;
    else if(addr >= 0xEAB2) res = 0xEAB2;
    else if(addr >= 0xEAAC) res = 0xEAAC;
    else if(addr >= 0xEAA0) res = 0xEAA0;
    else if(addr >= 0xEA90) res = 0xEA90;
    else if(addr >= 0xEA8F) res = 0xEA8F;
    else if(addr >= 0xEA8E) res = 0xEA8E;
    else if(addr >= 0xEA7F) res = 0xEA7F;
    else if(addr >= 0xEA7E) res = 0xEA7E;
    else if(addr >= 0xEA7D) res = 0xEA7D;
    else if(addr >= 0xEA75) res = 0xEA75;
    else if(addr >= 0xEA74) res = 0xEA74;
    else if(addr >= 0xEA71) res = 0xEA71;
    else if(addr >= 0xEA6E) res = 0xEA6E;
    else if(addr >= 0xEA6B) res = 0xEA6B;
    else if(addr >= 0xEA63) res = 0xEA63;
    else if(addr >= 0xEA62) res = 0xEA62;
    else if(addr >= 0xEA59) res = 0xEA59;
    else if(addr >= 0xEA56) res = 0xEA56;
    else if(addr >= 0xEA4E) res = 0xEA4E;
    else if(addr >= 0xEA44) res = 0xEA44;
    else if(addr >= 0xEA39) res = 0xEA39;
    else if(addr >= 0xEA2E) res = 0xEA2E;
    else if(addr >= 0xEA1A) res = 0xEA1A;
    else if(addr >= 0xEA0B) res = 0xEA0B;
    else if(addr >= 0xEA07) res = 0xEA07;
    else if(addr >= 0xE9FD) res = 0xE9FD;
    else if(addr >= 0xE9F2) res = 0xE9F2;
    else if(addr >= 0xE9DF) res = 0xE9DF;
    else if(addr >= 0xE9CD) res = 0xE9CD;
    else if(addr >= 0xE9C9) res = 0xE9C9;
    else if(addr >= 0xE9C0) res = 0xE9C0;
    else if(addr >= 0xE9B7) res = 0xE9B7;
    else if(addr >= 0xE9AE) res = 0xE9AE;
    else if(addr >= 0xE9A5) res = 0xE9A5;
    else if(addr >= 0xE99C) res = 0xE99C;
    else if(addr >= 0xE999) res = 0xE999;
    else if(addr >= 0xE987) res = 0xE987;
    else if(addr >= 0xE980) res = 0xE980;
    else if(addr >= 0xE976) res = 0xE976;
    else if(addr >= 0xE973) res = 0xE973;
    else if(addr >= 0xE963) res = 0xE963;
    else if(addr >= 0xE95C) res = 0xE95C;
    else if(addr >= 0xE94B) res = 0xE94B;
    else if(addr >= 0xE941) res = 0xE941;
    else if(addr >= 0xE937) res = 0xE937;
    else if(addr >= 0xE925) res = 0xE925;
    else if(addr >= 0xE916) res = 0xE916;
    else if(addr >= 0xE915) res = 0xE915;
    else if(addr >= 0xE90F) res = 0xE90F;
    else if(addr >= 0xE909) res = 0xE909;
    else if(addr >= 0xE902) res = 0xE902;
    else if(addr >= 0xE8FD) res = 0xE8FD;
    else if(addr >= 0xE8FA) res = 0xE8FA;
    else if(addr >= 0xE8ED) res = 0xE8ED;
    else if(addr >= 0xE8D7) res = 0xE8D7;
    else if(addr >= 0xE8D2) res = 0xE8D2;
    else if(addr >= 0xE8B7) res = 0xE8B7;
    else if(addr >= 0xE8A9) res = 0xE8A9;
    else if(addr >= 0xE89B) res = 0xE89B;
    else if(addr >= 0xE891) res = 0xE891;
    else if(addr >= 0xE87B) res = 0xE87B;
    else if(addr >= 0xE85B) res = 0xE85B;
    else if(addr >= 0xE853) res = 0xE853;
    else if(addr >= 0xE84B) res = 0xE84B;
    else if(addr >= 0xE848) res = 0xE848;
    else if(addr >= 0xE839) res = 0xE839;
    else if(addr >= 0xE82C) res = 0xE82C;
    else if(addr >= 0xE817) res = 0xE817;
    else if(addr >= 0xE802) res = 0xE802;
    else if(addr >= 0xE7FA) res = 0xE7FA;
    else if(addr >= 0xE7D8) res = 0xE7D8;
    else if(addr >= 0xE7C5) res = 0xE7C5;
    else if(addr >= 0xE7A8) res = 0xE7A8;
    else if(addr >= 0xE7A3) res = 0xE7A3;
    else if(addr >= 0xE78E) res = 0xE78E;
    else if(addr >= 0xE780) res = 0xE780;
    else if(addr >= 0xE77F) res = 0xE77F;
    else if(addr >= 0xE77E) res = 0xE77E;
    else if(addr >= 0xE775) res = 0xE775;
    else if(addr >= 0xE76D) res = 0xE76D;
    else if(addr >= 0xE767) res = 0xE767;
    else if(addr >= 0xE763) res = 0xE763;
    else if(addr >= 0xE754) res = 0xE754;
    else if(addr >= 0xE74D) res = 0xE74D;
    else if(addr >= 0xE742) res = 0xE742;
    else if(addr >= 0xE73D) res = 0xE73D;
    else if(addr >= 0xE739) res = 0xE739;
    else if(addr >= 0xE735) res = 0xE735;
    else if(addr >= 0xE727) res = 0xE727;
    else if(addr >= 0xE722) res = 0xE722;
    else if(addr >= 0xE718) res = 0xE718;
    else if(addr >= 0xE706) res = 0xE706;
    else if(addr >= 0xE6C7) res = 0xE6C7;
    else if(addr >= 0xE6C1) res = 0xE6C1;
    else if(addr >= 0xE6BC) res = 0xE6BC;
    else if(addr >= 0xE6B4) res = 0xE6B4;
    else if(addr >= 0xE6AB) res = 0xE6AB;
    else if(addr >= 0xE6AA) res = 0xE6AA;
    else if(addr >= 0xE69F) res = 0xE69F;
    else if(addr >= 0xE69B) res = 0xE69B;
    else if(addr >= 0xE698) res = 0xE698;
    else if(addr >= 0xE68E) res = 0xE68E;
    else if(addr >= 0xE688) res = 0xE688;
    else if(addr >= 0xE680) res = 0xE680;
    else if(addr >= 0xE648) res = 0xE648;
    else if(addr >= 0xE645) res = 0xE645;
    else if(addr >= 0xE644) res = 0xE644;
    else if(addr >= 0xE62D) res = 0xE62D;
    else if(addr >= 0xE627) res = 0xE627;
    else if(addr >= 0xE625) res = 0xE625;
    else if(addr >= 0xE60A) res = 0xE60A;
    else if(addr >= 0xE4DE) res = 0xE4DE;
    else if(addr >= 0xE4D1) res = 0xE4D1;
    else if(addr >= 0xE4AC) res = 0xE4AC;
    else if(addr >= 0xE44E) res = 0xE44E;
    else if(addr >= 0xE444) res = 0xE444;
    else if(addr >= 0xE418) res = 0xE418;
    else if(addr >= 0xE3F9) res = 0xE3F9;
    else if(addr >= 0xE3D4) res = 0xE3D4;
    else if(addr >= 0xE3C8) res = 0xE3C8;
    else if(addr >= 0xE3B6) res = 0xE3B6;
    else if(addr >= 0xE39D) res = 0xE39D;
    else if(addr >= 0xE38F) res = 0xE38F;
    else if(addr >= 0xE372) res = 0xE372;
    else if(addr >= 0xE368) res = 0xE368;
    else if(addr >= 0xE363) res = 0xE363;
    else if(addr >= 0xE355) res = 0xE355;
    else if(addr >= 0xE33B) res = 0xE33B;
    else if(addr >= 0xE31C) res = 0xE31C;
    else if(addr >= 0xE31B) res = 0xE31B;
    else if(addr >= 0xE318) res = 0xE318;
    else if(addr >= 0xE304) res = 0xE304;
    else if(addr >= 0xE303) res = 0xE303;
    else if(addr >= 0xE2F1) res = 0xE2F1;
    else if(addr >= 0xE2E2) res = 0xE2E2;
    else if(addr >= 0xE2DC) res = 0xE2DC;
    else if(addr >= 0xE2D3) res = 0xE2D3;
    else if(addr >= 0xE2D0) res = 0xE2D0;
    else if(addr >= 0xE2C2) res = 0xE2C2;
    else if(addr >= 0xE2BF) res = 0xE2BF;
    else if(addr >= 0xE2AA) res = 0xE2AA;
    else if(addr >= 0xE29C) res = 0xE29C;
    else if(addr >= 0xE291) res = 0xE291;
    else if(addr >= 0xE289) res = 0xE289;
    else if(addr >= 0xE275) res = 0xE275;
    else if(addr >= 0xE272) res = 0xE272;
    else if(addr >= 0xE265) res = 0xE265;
    else if(addr >= 0xE253) res = 0xE253;
    else if(addr >= 0xE228) res = 0xE228;
    else if(addr >= 0xE219) res = 0xE219;
    else if(addr >= 0xE207) res = 0xE207;
    else if(addr >= 0xE202) res = 0xE202;
    else if(addr >= 0xE1EF) res = 0xE1EF;
    else if(addr >= 0xE1DC) res = 0xE1DC;
    else if(addr >= 0xE1D8) res = 0xE1D8;
    else if(addr >= 0xE1CE) res = 0xE1CE;
    else if(addr >= 0xE1C8) res = 0xE1C8;
    else if(addr >= 0xE1C4) res = 0xE1C4;
    else if(addr >= 0xE1B7) res = 0xE1B7;
    else if(addr >= 0xE1B2) res = 0xE1B2;
    else if(addr >= 0xE1AC) res = 0xE1AC;
    else if(addr >= 0xE1A4) res = 0xE1A4;
    else if(addr >= 0xE19D) res = 0xE19D;
    else if(addr >= 0xE17E) res = 0xE17E;
    else if(addr >= 0xE16E) res = 0xE16E;
    else if(addr >= 0xE15E) res = 0xE15E;
    else if(addr >= 0xE153) res = 0xE153;
    else if(addr >= 0xE14D) res = 0xE14D;
    else if(addr >= 0xE13D) res = 0xE13D;
    else if(addr >= 0xE13B) res = 0xE13B;
    else if(addr >= 0xE138) res = 0xE138;
    else if(addr >= 0xE127) res = 0xE127;
    else if(addr >= 0xE120) res = 0xE120;
    else if(addr >= 0xE115) res = 0xE115;
    else if(addr >= 0xE105) res = 0xE105;
    else if(addr >= 0xE104) res = 0xE104;
    else if(addr >= 0xE0F3) res = 0xE0F3;
    else if(addr >= 0xE0E2) res = 0xE0E2;
    else if(addr >= 0xE0E1) res = 0xE0E1;
    else if(addr >= 0xE0D9) res = 0xE0D9;
    else if(addr >= 0xE0D6) res = 0xE0D6;
    else if(addr >= 0xE0C8) res = 0xE0C8;
    else if(addr >= 0xE0BC) res = 0xE0BC;
    else if(addr >= 0xE0B7) res = 0xE0B7;
    else if(addr >= 0xE0B2) res = 0xE0B2;
    else if(addr >= 0xE0AB) res = 0xE0AB;
    else if(addr >= 0xE0AA) res = 0xE0AA;
    else if(addr >= 0xE0A3) res = 0xE0A3;
    else if(addr >= 0xE09E) res = 0xE09E;
    else if(addr >= 0xE096) res = 0xE096;
    else if(addr >= 0xE094) res = 0xE094;
    else if(addr >= 0xE07C) res = 0xE07C;
    else if(addr >= 0xE07B) res = 0xE07B;
    else if(addr >= 0xE06B) res = 0xE06B;
    else if(addr >= 0xE05D) res = 0xE05D;
    else if(addr >= 0xE03C) res = 0xE03C;
    else if(addr >= 0xE035) res = 0xE035;
    else if(addr >= 0xE034) res = 0xE034;
    else if(addr >= 0xE02A) res = 0xE02A;
    else if(addr >= 0xE01D) res = 0xE01D;
    else if(addr >= 0xE018) res = 0xE018;
    else if(addr >= 0xE009) res = 0xE009;
    else if(addr >= 0xDFF6) res = 0xDFF6;
    else if(addr >= 0xDFE4) res = 0xDFE4;
    else if(addr >= 0xDFD0) res = 0xDFD0;
    else if(addr >= 0xDFCD) res = 0xDFCD;
    else if(addr >= 0xDFC2) res = 0xDFC2;
    else if(addr >= 0xDFBF) res = 0xDFBF;
    else if(addr >= 0xDFB7) res = 0xDFB7;
    else if(addr >= 0xDFB0) res = 0xDFB0;
    else if(addr >= 0xDFA0) res = 0xDFA0;
    else if(addr >= 0xDF9E) res = 0xDF9E;
    else if(addr >= 0xDF9B) res = 0xDF9B;
    else if(addr >= 0xDF93) res = 0xDF93;
    else if(addr >= 0xDF8F) res = 0xDF8F;
    else if(addr >= 0xDF8B) res = 0xDF8B;
    else if(addr >= 0xDF7B) res = 0xDF7B;
    else if(addr >= 0xDF77) res = 0xDF77;
    else if(addr >= 0xDF66) res = 0xDF66;
    else if(addr >= 0xDF65) res = 0xDF65;
    else if(addr >= 0xDF5C) res = 0xDF5C;
    else if(addr >= 0xDF51) res = 0xDF51;
    else if(addr >= 0xDF4C) res = 0xDF4C;
    else if(addr >= 0xDF45) res = 0xDF45;
    else if(addr >= 0xDF25) res = 0xDF25;
    else if(addr >= 0xDF21) res = 0xDF21;
    else if(addr >= 0xDF1B) res = 0xDF1B;
    else if(addr >= 0xDF12) res = 0xDF12;
    else if(addr >= 0xDF0B) res = 0xDF0B;
    else if(addr >= 0xDEF8) res = 0xDEF8;
    else if(addr >= 0xDEE9) res = 0xDEE9;
    else if(addr >= 0xDEDC) res = 0xDEDC;
    else if(addr >= 0xDED2) res = 0xDED2;
    else if(addr >= 0xDECC) res = 0xDECC;
    else if(addr >= 0xDEC1) res = 0xDEC1;
    else if(addr >= 0xDEB6) res = 0xDEB6;
    else if(addr >= 0xDEA5) res = 0xDEA5;
    else if(addr >= 0xDE95) res = 0xDE95;
    else if(addr >= 0xDE8B) res = 0xDE8B;
    else if(addr >= 0xDE7F) res = 0xDE7F;
    else if(addr >= 0xDE75) res = 0xDE75;
    else if(addr >= 0xDE73) res = 0xDE73;
    else if(addr >= 0xDE6C) res = 0xDE6C;
    else if(addr >= 0xDE65) res = 0xDE65;
    else if(addr >= 0xDE5E) res = 0xDE5E;
    else if(addr >= 0xDE57) res = 0xDE57;
    else if(addr >= 0xDE50) res = 0xDE50;
    else if(addr >= 0xDE3E) res = 0xDE3E;
    else if(addr >= 0xDE3B) res = 0xDE3B;
    else if(addr >= 0xDE2B) res = 0xDE2B;
    else if(addr >= 0xDE19) res = 0xDE19;
    else if(addr >= 0xDE0C) res = 0xDE0C;
    else if(addr >= 0xDDFD) res = 0xDDFD;
    else if(addr >= 0xDDFC) res = 0xDDFC;
    else if(addr >= 0xDDF1) res = 0xDDF1;
    else if(addr >= 0xDDCA) res = 0xDDCA;
    else if(addr >= 0xDDC9) res = 0xDDC9;
    else if(addr >= 0xDDC2) res = 0xDDC2;
    else if(addr >= 0xDDB9) res = 0xDDB9;
    else if(addr >= 0xDDB7) res = 0xDDB7;
    else if(addr >= 0xDDAB) res = 0xDDAB;
    else if(addr >= 0xDDA6) res = 0xDDA6;
    else if(addr >= 0xDDA3) res = 0xDDA3;
    else if(addr >= 0xDD9D) res = 0xDD9D;
    else if(addr >= 0xDD97) res = 0xDD97;
    else if(addr >= 0xDD95) res = 0xDD95;
    else if(addr >= 0xDD8D) res = 0xDD8D;
    else if(addr >= 0xDD16) res = 0xDD16;
    else if(addr >= 0xDCFD) res = 0xDCFD;
    else if(addr >= 0xDCDA) res = 0xDCDA;
    else if(addr >= 0xDCB6) res = 0xDCB6;
    else if(addr >= 0xDCA9) res = 0xDCA9;
    else if(addr >= 0xDC98) res = 0xDC98;
    else if(addr >= 0xDC81) res = 0xDC81;
    else if(addr >= 0xDC65) res = 0xDC65;
    else if(addr >= 0xDC46) res = 0xDC46;
    else if(addr >= 0xDC29) res = 0xDC29;
    else if(addr >= 0xDC21) res = 0xDC21;
    else if(addr >= 0xDC06) res = 0xDC06;
    else if(addr >= 0xDBA5) res = 0xDBA5;
    else if(addr >= 0xDB8C) res = 0xDB8C;
    else if(addr >= 0xDB88) res = 0xDB88;
    else if(addr >= 0xDB76) res = 0xDB76;
    else if(addr >= 0xDB62) res = 0xDB62;
    else if(addr >= 0xDB5F) res = 0xDB5F;
    else if(addr >= 0xDB2C) res = 0xDB2C;
    else if(addr >= 0xDB29) res = 0xDB29;
    else if(addr >= 0xDB26) res = 0xDB26;
    else if(addr >= 0xDB0C) res = 0xDB0C;
    else if(addr >= 0xDB02) res = 0xDB02;
    else if(addr >= 0xDAFF) res = 0xDAFF;
    else if(addr >= 0xDAF0) res = 0xDAF0;
    else if(addr >= 0xDAEC) res = 0xDAEC;
    else if(addr >= 0xDAE9) res = 0xDAE9;
    else if(addr >= 0xDAD4) res = 0xDAD4;
    else if(addr >= 0xDAD1) res = 0xDAD1;
    else if(addr >= 0xDAC0) res = 0xDAC0;
    else if(addr >= 0xDAA7) res = 0xDAA7;
    else if(addr >= 0xDA9E) res = 0xDA9E;
    else if(addr >= 0xDA90) res = 0xDA90;
    else if(addr >= 0xDA86) res = 0xDA86;
    else if(addr >= 0xDA6D) res = 0xDA6D;
    else if(addr >= 0xDA62) res = 0xDA62;
    else if(addr >= 0xDA55) res = 0xDA55;
    else if(addr >= 0xDA42) res = 0xDA42;
    else if(addr >= 0xDA2A) res = 0xDA2A;
    else if(addr >= 0xDA29) res = 0xDA29;
    else if(addr >= 0xDA1C) res = 0xDA1C;
    else if(addr >= 0xDA11) res = 0xDA11;
    else if(addr >= 0xDA09) res = 0xDA09;
    else if(addr >= 0xDA06) res = 0xDA06;
    else if(addr >= 0xD9EF) res = 0xD9EF;
    else if(addr >= 0xD9E3) res = 0xD9E3;
    else if(addr >= 0xD9C3) res = 0xD9C3;
    else if(addr >= 0xD9A0) res = 0xD9A0;
    else if(addr >= 0xD990) res = 0xD990;
    else if(addr >= 0xD96A) res = 0xD96A;
    else if(addr >= 0xD965) res = 0xD965;
    else if(addr >= 0xD95C) res = 0xD95C;
    else if(addr >= 0xD94A) res = 0xD94A;
    else if(addr >= 0xD945) res = 0xD945;
    else if(addr >= 0xD940) res = 0xD940;
    else if(addr >= 0xD8F5) res = 0xD8F5;
    else if(addr >= 0xD8F0) res = 0xD8F0;
    else if(addr >= 0xD8E1) res = 0xD8E1;
    else if(addr >= 0xD8D9) res = 0xD8D9;
    else if(addr >= 0xD8CD) res = 0xD8CD;
    else if(addr >= 0xD8C6) res = 0xD8C6;
    else if(addr >= 0xD8B1) res = 0xD8B1;
    else if(addr >= 0xD8A7) res = 0xD8A7;
    else if(addr >= 0xD891) res = 0xD891;
    else if(addr >= 0xD876) res = 0xD876;
    else if(addr >= 0xD840) res = 0xD840;
    else if(addr >= 0xD83C) res = 0xD83C;
    else if(addr >= 0xD837) res = 0xD837;
    else if(addr >= 0xD834) res = 0xD834;
    else if(addr >= 0xD82B) res = 0xD82B;
    else if(addr >= 0xD81C) res = 0xD81C;
    else if(addr >= 0xD815) res = 0xD815;
    else if(addr >= 0xD7FF) res = 0xD7FF;
    else if(addr >= 0xD7F3) res = 0xD7F3;
    else if(addr >= 0xD7EB) res = 0xD7EB;
    else if(addr >= 0xD7CF) res = 0xD7CF;
    else if(addr >= 0xD7B4) res = 0xD7B4;
    else if(addr >= 0xD790) res = 0xD790;
    else if(addr >= 0xD74D) res = 0xD74D;
    else if(addr >= 0xD73D) res = 0xD73D;
    else if(addr >= 0xD730) res = 0xD730;
    else if(addr >= 0xD726) res = 0xD726;
    else if(addr >= 0xD715) res = 0xD715;
    else if(addr >= 0xD6E4) res = 0xD6E4;
    else if(addr >= 0xD6D0) res = 0xD6D0;
    else if(addr >= 0xD6C4) res = 0xD6C4;
    else if(addr >= 0xD6B9) res = 0xD6B9;
    else if(addr >= 0xD6AB) res = 0xD6AB;
    else if(addr >= 0xD6A6) res = 0xD6A6;
    else if(addr >= 0xD69A) res = 0xD69A;
    else if(addr >= 0xD693) res = 0xD693;
    else if(addr >= 0xD692) res = 0xD692;
    else if(addr >= 0xD688) res = 0xD688;
    else if(addr >= 0xD67C) res = 0xD67C;
    else if(addr >= 0xD676) res = 0xD676;
    else if(addr >= 0xD66D) res = 0xD66D;
    else if(addr >= 0xD65C) res = 0xD65C;
    else if(addr >= 0xD651) res = 0xD651;
    else if(addr >= 0xD645) res = 0xD645;
    else if(addr >= 0xD644) res = 0xD644;
    else if(addr >= 0xD63F) res = 0xD63F;
    else if(addr >= 0xD635) res = 0xD635;
    else if(addr >= 0xD631) res = 0xD631;
    else if(addr >= 0xD625) res = 0xD625;
    else if(addr >= 0xD600) res = 0xD600;
    else if(addr >= 0xD5F4) res = 0xD5F4;
    else if(addr >= 0xD5E3) res = 0xD5E3;
    else if(addr >= 0xD5C6) res = 0xD5C6;
    else if(addr >= 0xD5C4) res = 0xD5C4;
    else if(addr >= 0xD5C2) res = 0xD5C2;
    else if(addr >= 0xD5BA) res = 0xD5BA;
    else if(addr >= 0xD5A6) res = 0xD5A6;
    else if(addr >= 0xD599) res = 0xD599;
    else if(addr >= 0xD593) res = 0xD593;
    else if(addr >= 0xD590) res = 0xD590;
    else if(addr >= 0xD58C) res = 0xD58C;
    else if(addr >= 0xD58A) res = 0xD58A;
    else if(addr >= 0xD586) res = 0xD586;
    else if(addr >= 0xD57A) res = 0xD57A;
    else if(addr >= 0xD572) res = 0xD572;
    else if(addr >= 0xD55F) res = 0xD55F;
    else if(addr >= 0xD552) res = 0xD552;
    else if(addr >= 0xD54D) res = 0xD54D;
    else if(addr >= 0xD54A) res = 0xD54A;
    else if(addr >= 0xD53F) res = 0xD53F;
    else if(addr >= 0xD538) res = 0xD538;
    else if(addr >= 0xD535) res = 0xD535;
    else if(addr >= 0xD50E) res = 0xD50E;
    else if(addr >= 0xD506) res = 0xD506;
    else if(addr >= 0xD4F6) res = 0xD4F6;
    else if(addr >= 0xD4EB) res = 0xD4EB;
    else if(addr >= 0xD4E8) res = 0xD4E8;
    else if(addr >= 0xD4DA) res = 0xD4DA;
    else if(addr >= 0xD4C8) res = 0xD4C8;
    else if(addr >= 0xD4BB) res = 0xD4BB;
    else if(addr >= 0xD48D) res = 0xD48D;
    else if(addr >= 0xD486) res = 0xD486;
    else if(addr >= 0xD477) res = 0xD477;
    else if(addr >= 0xD475) res = 0xD475;
    else if(addr >= 0xD466) res = 0xD466;
    else if(addr >= 0xD464) res = 0xD464;
    else if(addr >= 0xD460) res = 0xD460;
    else if(addr >= 0xD45F) res = 0xD45F;
    else if(addr >= 0xD44D) res = 0xD44D;
    else if(addr >= 0xD445) res = 0xD445;
    else if(addr >= 0xD443) res = 0xD443;
    else if(addr >= 0xD43A) res = 0xD43A;
    else if(addr >= 0xD433) res = 0xD433;
    else if(addr >= 0xD414) res = 0xD414;
    else if(addr >= 0xD409) res = 0xD409;
    else if(addr >= 0xD403) res = 0xD403;
    else if(addr >= 0xD400) res = 0xD400;
    else if(addr >= 0xD3FF) res = 0xD3FF;
    else if(addr >= 0xD3F0) res = 0xD3F0;
    else if(addr >= 0xD3EE) res = 0xD3EE;
    else if(addr >= 0xD3EC) res = 0xD3EC;
    else if(addr >= 0xD3DE) res = 0xD3DE;
    else if(addr >= 0xD3D7) res = 0xD3D7;
    else if(addr >= 0xD3D3) res = 0xD3D3;
    else if(addr >= 0xD3CE) res = 0xD3CE;
    else if(addr >= 0xD3B4) res = 0xD3B4;
    else if(addr >= 0xD3AA) res = 0xD3AA;
    else if(addr >= 0xD39B) res = 0xD39B;
    else if(addr >= 0xD391) res = 0xD391;
    else if(addr >= 0xD383) res = 0xD383;
    else if(addr >= 0xD37F) res = 0xD37F;
    else if(addr >= 0xD37A) res = 0xD37A;
    else if(addr >= 0xD373) res = 0xD373;
    else if(addr >= 0xD363) res = 0xD363;
    else if(addr >= 0xD35E) res = 0xD35E;
    else if(addr >= 0xD355) res = 0xD355;
    else if(addr >= 0xD348) res = 0xD348;
    else if(addr >= 0xD33E) res = 0xD33E;
    else if(addr >= 0xD339) res = 0xD339;
    else if(addr >= 0xD317) res = 0xD317;
    else if(addr >= 0xD313) res = 0xD313;
    else if(addr >= 0xD30B) res = 0xD30B;
    else if(addr >= 0xD307) res = 0xD307;
    else if(addr >= 0xD2F9) res = 0xD2F9;
    else if(addr >= 0xD2F3) res = 0xD2F3;
    else if(addr >= 0xD2E9) res = 0xD2E9;
    else if(addr >= 0xD2DA) res = 0xD2DA;
    else if(addr >= 0xD2D9) res = 0xD2D9;
    else if(addr >= 0xD2D8) res = 0xD2D8;
    else if(addr >= 0xD2C8) res = 0xD2C8;
    else if(addr >= 0xD2BC) res = 0xD2BC;
    else if(addr >= 0xD2BA) res = 0xD2BA;
    else if(addr >= 0xD2B6) res = 0xD2B6;
    else if(addr >= 0xD2A3) res = 0xD2A3;
    else if(addr >= 0xD28E) res = 0xD28E;
    else if(addr >= 0xD28D) res = 0xD28D;
    else if(addr >= 0xD27C) res = 0xD27C;
    else if(addr >= 0xD26B) res = 0xD26B;
    else if(addr >= 0xD25A) res = 0xD25A;
    else if(addr >= 0xD259) res = 0xD259;
    else if(addr >= 0xD253) res = 0xD253;
    else if(addr >= 0xD24D) res = 0xD24D;
    else if(addr >= 0xD249) res = 0xD249;
    else if(addr >= 0xD22E) res = 0xD22E;
    else if(addr >= 0xD227) res = 0xD227;
    else if(addr >= 0xD226) res = 0xD226;
    else if(addr >= 0xD217) res = 0xD217;
    else if(addr >= 0xD20F) res = 0xD20F;
    else if(addr >= 0xD206) res = 0xD206;
    else if(addr >= 0xD1F5) res = 0xD1F5;
    else if(addr >= 0xD1F3) res = 0xD1F3;
    else if(addr >= 0xD1E3) res = 0xD1E3;
    else if(addr >= 0xD1E2) res = 0xD1E2;
    else if(addr >= 0xD1DF) res = 0xD1DF;
    else if(addr >= 0xD1D3) res = 0xD1D3;
    else if(addr >= 0xD1C6) res = 0xD1C6;
    else if(addr >= 0xD1A3) res = 0xD1A3;
    else if(addr >= 0xD19D) res = 0xD19D;
    else if(addr >= 0xD192) res = 0xD192;
    else if(addr >= 0xD191) res = 0xD191;
    else if(addr >= 0xD16A) res = 0xD16A;
    else if(addr >= 0xD164) res = 0xD164;
    else if(addr >= 0xD156) res = 0xD156;
    else if(addr >= 0xD151) res = 0xD151;
    else if(addr >= 0xD14D) res = 0xD14D;
    else if(addr >= 0xD137) res = 0xD137;
    else if(addr >= 0xD12F) res = 0xD12F;
    else if(addr >= 0xD125) res = 0xD125;
    else if(addr >= 0xD123) res = 0xD123;
    else if(addr >= 0xD121) res = 0xD121;
    else if(addr >= 0xD119) res = 0xD119;
    else if(addr >= 0xD10F) res = 0xD10F;
    else if(addr >= 0xD107) res = 0xD107;
    else if(addr >= 0xD106) res = 0xD106;
    else if(addr >= 0xD0F9) res = 0xD0F9;
    else if(addr >= 0xD0F3) res = 0xD0F3;
    else if(addr >= 0xD0EB) res = 0xD0EB;
    else if(addr >= 0xD0E8) res = 0xD0E8;
    else if(addr >= 0xD0C9) res = 0xD0C9;
    else if(addr >= 0xD0C7) res = 0xD0C7;
    else if(addr >= 0xD0C3) res = 0xD0C3;
    else if(addr >= 0xD0B7) res = 0xD0B7;
    else if(addr >= 0xD0AF) res = 0xD0AF;
    else if(addr >= 0xD09B) res = 0xD09B;
    else if(addr >= 0xD083) res = 0xD083;
    else if(addr >= 0xD07D) res = 0xD07D;
    else if(addr >= 0xD075) res = 0xD075;
    else if(addr >= 0xD042) res = 0xD042;
    else if(addr >= 0xD02C) res = 0xD02C;
    else if(addr >= 0xD024) res = 0xD024;
    else if(addr >= 0xD00E) res = 0xD00E;
    else if(addr >= 0xD00B) res = 0xD00B;
    else if(addr >= 0xD005) res = 0xD005;
    else if(addr >= 0xCFFD) res = 0xCFFD;
    else if(addr >= 0xCFF1) res = 0xCFF1;
    else if(addr >= 0xCFED) res = 0xCFED;
    else if(addr >= 0xCFE8) res = 0xCFE8;
    else if(addr >= 0xCFD8) res = 0xCFD8;
    else if(addr >= 0xCFCE) res = 0xCFCE;
    else if(addr >= 0xCFC9) res = 0xCFC9;
    else if(addr >= 0xCFBF) res = 0xCFBF;
    else if(addr >= 0xCFB7) res = 0xCFB7;
    else if(addr >= 0xCFAF) res = 0xCFAF;
    else if(addr >= 0xCF9B) res = 0xCF9B;
    else if(addr >= 0xCF8C) res = 0xCF8C;
    else if(addr >= 0xCF8B) res = 0xCF8B;
    else if(addr >= 0xCF7B) res = 0xCF7B;
    else if(addr >= 0xCF76) res = 0xCF76;
    else if(addr >= 0xCF6F) res = 0xCF6F;
    else if(addr >= 0xCF6C) res = 0xCF6C;
    else if(addr >= 0xCF66) res = 0xCF66;
    else if(addr >= 0xCF5D) res = 0xCF5D;
    else if(addr >= 0xCF57) res = 0xCF57;
    else if(addr >= 0xCF1E) res = 0xCF1E;
    else if(addr >= 0xCF1D) res = 0xCF1D;
    else if(addr >= 0xCF0D) res = 0xCF0D;
    else if(addr >= 0xCF09) res = 0xCF09;
    else if(addr >= 0xCEFC) res = 0xCEFC;
    else if(addr >= 0xCEFA) res = 0xCEFA;
    else if(addr >= 0xCEF0) res = 0xCEF0;
    else if(addr >= 0xCEED) res = 0xCEED;
    else if(addr >= 0xCEE6) res = 0xCEE6;
    else if(addr >= 0xCEE5) res = 0xCEE5;
    else if(addr >= 0xCEE2) res = 0xCEE2;
    else if(addr >= 0xCED9) res = 0xCED9;
    else if(addr >= 0xCED6) res = 0xCED6;
    else if(addr >= 0xCEBF) res = 0xCEBF;
    else if(addr >= 0xCEB0) res = 0xCEB0;
    else if(addr >= 0xCEA3) res = 0xCEA3;
    else if(addr >= 0xCE89) res = 0xCE89;
    else if(addr >= 0xCE87) res = 0xCE87;
    else if(addr >= 0xCE71) res = 0xCE71;
    else if(addr >= 0xCE6E) res = 0xCE6E;
    else if(addr >= 0xCE6D) res = 0xCE6D;
    else if(addr >= 0xCE57) res = 0xCE57;
    else if(addr >= 0xCE50) res = 0xCE50;
    else if(addr >= 0xCE4C) res = 0xCE4C;
    else if(addr >= 0xCE41) res = 0xCE41;
    else if(addr >= 0xCE2C) res = 0xCE2C;
    else if(addr >= 0xCE0E) res = 0xCE0E;
    else if(addr >= 0xCDF5) res = 0xCDF5;
    else if(addr >= 0xCDF2) res = 0xCDF2;
    else if(addr >= 0xCDE5) res = 0xCDE5;
    else if(addr >= 0xCDE0) res = 0xCDE0;
    else if(addr >= 0xCDD2) res = 0xCDD2;
    else if(addr >= 0xCDBD) res = 0xCDBD;
    else if(addr >= 0xCDBA) res = 0xCDBA;
    else if(addr >= 0xCDA3) res = 0xCDA3;
    else if(addr >= 0xCD97) res = 0xCD97;
    else if(addr >= 0xCD8C) res = 0xCD8C;
    else if(addr >= 0xCD81) res = 0xCD81;
    else if(addr >= 0xCD73) res = 0xCD73;
    else if(addr >= 0xCD5F) res = 0xCD5F;
    else if(addr >= 0xCD56) res = 0xCD56;
    else if(addr >= 0xCD42) res = 0xCD42;
    else if(addr >= 0xCD3C) res = 0xCD3C;
    else if(addr >= 0xCD36) res = 0xCD36;
    else if(addr >= 0xCD31) res = 0xCD31;
    else if(addr >= 0xCD2C) res = 0xCD2C;
    else if(addr >= 0xCD1A) res = 0xCD1A;
    else if(addr >= 0xCD19) res = 0xCD19;
    else if(addr >= 0xCD03) res = 0xCD03;
    else if(addr >= 0xCCF5) res = 0xCCF5;
    else if(addr >= 0xCCF2) res = 0xCCF2;
    else if(addr >= 0xCCE4) res = 0xCCE4;
    else if(addr >= 0xCCD7) res = 0xCCD7;
    else if(addr >= 0xCCD0) res = 0xCCD0;
    else if(addr >= 0xCCCA) res = 0xCCCA;
    else if(addr >= 0xCCAB) res = 0xCCAB;
    else if(addr >= 0xCCA1) res = 0xCCA1;
    else if(addr >= 0xCC92) res = 0xCC92;
    else if(addr >= 0xCC8B) res = 0xCC8B;
    else if(addr >= 0xCC7C) res = 0xCC7C;
    else if(addr >= 0xCC6F) res = 0xCC6F;
    else if(addr >= 0xCC42) res = 0xCC42;
    else if(addr >= 0xCC38) res = 0xCC38;
    else if(addr >= 0xCC30) res = 0xCC30;
    else if(addr >= 0xCC2B) res = 0xCC2B;
    else if(addr >= 0xCC26) res = 0xCC26;
    else if(addr >= 0xCC1B) res = 0xCC1B;
    else if(addr >= 0xCBF1) res = 0xCBF1;
    else if(addr >= 0xCBB8) res = 0xCBB8;
    else if(addr >= 0xCBA5) res = 0xCBA5;
    else if(addr >= 0xCBA0) res = 0xCBA0;
    else if(addr >= 0xCB84) res = 0xCB84;
    else if(addr >= 0xCB72) res = 0xCB72;
    else if(addr >= 0xCB6C) res = 0xCB6C;
    else if(addr >= 0xCB63) res = 0xCB63;
    else if(addr >= 0xCB5C) res = 0xCB5C;
    else if(addr >= 0xCB50) res = 0xCB50;
    else if(addr >= 0xCB4B) res = 0xCB4B;
    else if(addr >= 0xCB45) res = 0xCB45;
    else if(addr >= 0xCB2B) res = 0xCB2B;
    else if(addr >= 0xCB20) res = 0xCB20;
    else if(addr >= 0xCB1D) res = 0xCB1D;
    else if(addr >= 0xCAF8) res = 0xCAF8;
    else if(addr >= 0xCAF4) res = 0xCAF4;
    else if(addr >= 0xCAEA) res = 0xCAEA;
    else if(addr >= 0xCAE7) res = 0xCAE7;
    else if(addr >= 0xCAE6) res = 0xCAE6;
    else if(addr >= 0xCAD6) res = 0xCAD6;
    else if(addr >= 0xCACC) res = 0xCACC;
    else if(addr >= 0xCA97) res = 0xCA97;
    else if(addr >= 0xCA88) res = 0xCA88;
    else if(addr >= 0xCA53) res = 0xCA53;
    else if(addr >= 0xCA52) res = 0xCA52;
    else if(addr >= 0xCA39) res = 0xCA39;
    else if(addr >= 0xCA35) res = 0xCA35;
    else if(addr >= 0xCA31) res = 0xCA31;
    else if(addr >= 0xC9FA) res = 0xC9FA;
    else if(addr >= 0xC9EA) res = 0xC9EA;
    else if(addr >= 0xC9D8) res = 0xC9D8;
    else if(addr >= 0xC9D5) res = 0xC9D5;
    else if(addr >= 0xC9CE) res = 0xC9CE;
    else if(addr >= 0xC9B9) res = 0xC9B9;
    else if(addr >= 0xC9A7) res = 0xC9A7;
    else if(addr >= 0xC9A1) res = 0xC9A1;
    else if(addr >= 0xC987) res = 0xC987;
    else if(addr >= 0xC982) res = 0xC982;
    else if(addr >= 0xC952) res = 0xC952;
    else if(addr >= 0xC932) res = 0xC932;
    else if(addr >= 0xC928) res = 0xC928;
    else if(addr >= 0xC923) res = 0xC923;
    else if(addr >= 0xC90F) res = 0xC90F;
    else if(addr >= 0xC90C) res = 0xC90C;
    else if(addr >= 0xC8F0) res = 0xC8F0;
    else if(addr >= 0xC8EF) res = 0xC8EF;
    else if(addr >= 0xC8E0) res = 0xC8E0;
    else if(addr >= 0xC8C6) res = 0xC8C6;
    else if(addr >= 0xC8B6) res = 0xC8B6;
    else if(addr >= 0xC8AD) res = 0xC8AD;
    else if(addr >= 0xC894) res = 0xC894;
    else if(addr >= 0xC87D) res = 0xC87D;
    else if(addr >= 0xC872) res = 0xC872;
    else if(addr >= 0xC86D) res = 0xC86D;
    else if(addr >= 0xC86B) res = 0xC86B;
    else if(addr >= 0xC855) res = 0xC855;
    else if(addr >= 0xC835) res = 0xC835;
    else if(addr >= 0xC823) res = 0xC823;
    else if(addr >= 0xC817) res = 0xC817;
    else if(addr >= 0xC806) res = 0xC806;
    else if(addr >= 0xC7ED) res = 0xC7ED;
    else if(addr >= 0xC7E5) res = 0xC7E5;
    else if(addr >= 0xC7DC) res = 0xC7DC;
    else if(addr >= 0xC7B7) res = 0xC7B7;
    else if(addr >= 0xC7B0) res = 0xC7B0;
    else if(addr >= 0xC7AC) res = 0xC7AC;
    else if(addr >= 0xC7AB) res = 0xC7AB;
    else if(addr >= 0xC7A7) res = 0xC7A7;
    else if(addr >= 0xC783) res = 0xC783;
    else if(addr >= 0xC773) res = 0xC773;
    else if(addr >= 0xC76B) res = 0xC76B;
    else if(addr >= 0xC74A) res = 0xC74A;
    else if(addr >= 0xC73C) res = 0xC73C;
    else if(addr >= 0xC728) res = 0xC728;
    else if(addr >= 0xC71B) res = 0xC71B;
    else if(addr >= 0xC70E) res = 0xC70E;
    else if(addr >= 0xC6FC) res = 0xC6FC;
    else if(addr >= 0xC6F7) res = 0xC6F7;
    else if(addr >= 0xC6DE) res = 0xC6DE;
    else if(addr >= 0xC6CE) res = 0xC6CE;
    else if(addr >= 0xC6C8) res = 0xC6C8;
    else if(addr >= 0xC6AD) res = 0xC6AD;
    else if(addr >= 0xC6A6) res = 0xC6A6;
    else if(addr >= 0xC6A5) res = 0xC6A5;
    else if(addr >= 0xC697) res = 0xC697;
    else if(addr >= 0xC688) res = 0xC688;
    else if(addr >= 0xC687) res = 0xC687;
    else if(addr >= 0xC681) res = 0xC681;
    else if(addr >= 0xC66E) res = 0xC66E;
    else if(addr >= 0xC669) res = 0xC669;
    else if(addr >= 0xC65F) res = 0xC65F;
    else if(addr >= 0xC63D) res = 0xC63D;
    else if(addr >= 0xC62F) res = 0xC62F;
    else if(addr >= 0xC629) res = 0xC629;
    else if(addr >= 0xC617) res = 0xC617;
    else if(addr >= 0xC604) res = 0xC604;
    else if(addr >= 0xC5FB) res = 0xC5FB;
    else if(addr >= 0xC5D7) res = 0xC5D7;
    else if(addr >= 0xC5CA) res = 0xC5CA;
    else if(addr >= 0xC5C4) res = 0xC5C4;
    else if(addr >= 0xC5AC) res = 0xC5AC;
    else if(addr >= 0xC5A6) res = 0xC5A6;
    else if(addr >= 0xC59A) res = 0xC59A;
    else if(addr >= 0xC594) res = 0xC594;
    else if(addr >= 0xC589) res = 0xC589;
    else if(addr >= 0xC55C) res = 0xC55C;
    else if(addr >= 0xC535) res = 0xC535;
    else if(addr >= 0xC52B) res = 0xC52B;
    else if(addr >= 0xC51D) res = 0xC51D;
    else if(addr >= 0xC51B) res = 0xC51B;
    else if(addr >= 0xC50A) res = 0xC50A;
    else if(addr >= 0xC4FE) res = 0xC4FE;
    else if(addr >= 0xC4EC) res = 0xC4EC;
    else if(addr >= 0xC4E7) res = 0xC4E7;
    else if(addr >= 0xC4E6) res = 0xC4E6;
    else if(addr >= 0xC4D8) res = 0xC4D8;
    else if(addr >= 0xC4D7) res = 0xC4D7;
    else if(addr >= 0xC4C9) res = 0xC4C9;
    else if(addr >= 0xC4BA) res = 0xC4BA;
    else if(addr >= 0xC4B5) res = 0xC4B5;
    else if(addr >= 0xC4AA) res = 0xC4AA;
    else if(addr >= 0xC49D) res = 0xC49D;
    else if(addr >= 0xC492) res = 0xC492;
    else if(addr >= 0xC48B) res = 0xC48B;
    else if(addr >= 0xC485) res = 0xC485;
    else if(addr >= 0xC47E) res = 0xC47E;
    else if(addr >= 0xC475) res = 0xC475;
    else if(addr >= 0xC470) res = 0xC470;
    else if(addr >= 0xC462) res = 0xC462;
    else if(addr >= 0xC45C) res = 0xC45C;
    else if(addr >= 0xC452) res = 0xC452;
    else if(addr >= 0xC44F) res = 0xC44F;
    else if(addr >= 0xC440) res = 0xC440;
    else if(addr >= 0xC43C) res = 0xC43C;
    else if(addr >= 0xC439) res = 0xC439;
    else if(addr >= 0xC434) res = 0xC434;
    else if(addr >= 0xC420) res = 0xC420;
    else if(addr >= 0xC41B) res = 0xC41B;
    else if(addr >= 0xC400) res = 0xC400;
    else if(addr >= 0xC3EF) res = 0xC3EF;
    else if(addr >= 0xC3E8) res = 0xC3E8;
    else if(addr >= 0xC3D5) res = 0xC3D5;
    else if(addr >= 0xC3CA) res = 0xC3CA;
    else if(addr >= 0xC3C7) res = 0xC3C7;
    else if(addr >= 0xC3BD) res = 0xC3BD;
    else if(addr >= 0xC3B8) res = 0xC3B8;
    else if(addr >= 0xC3B0) res = 0xC3B0;
    else if(addr >= 0xC398) res = 0xC398;
    else if(addr >= 0xC38F) res = 0xC38F;
    else if(addr >= 0xC388) res = 0xC388;
    else if(addr >= 0xC383) res = 0xC383;
    else if(addr >= 0xC370) res = 0xC370;
    else if(addr >= 0xC368) res = 0xC368;
    else if(addr >= 0xC361) res = 0xC361;
    else if(addr >= 0xC352) res = 0xC352;
    else if(addr >= 0xC34F) res = 0xC34F;
    else if(addr >= 0xC34D) res = 0xC34D;
    else if(addr >= 0xC34C) res = 0xC34C;
    else if(addr >= 0xC33C) res = 0xC33C;
    else if(addr >= 0xC325) res = 0xC325;
    else if(addr >= 0xC320) res = 0xC320;
    else if(addr >= 0xC312) res = 0xC312;
    else if(addr >= 0xC2DC) res = 0xC2DC;
    else if(addr >= 0xC2CB) res = 0xC2CB;
    else if(addr >= 0xC2CA) res = 0xC2CA;
    else if(addr >= 0xC2B3) res = 0xC2B3;
    else if(addr >= 0xC2B1) res = 0xC2B1;
    else if(addr >= 0xC2A0) res = 0xC2A0;
    else if(addr >= 0xC29E) res = 0xC29E;
    else if(addr >= 0xC299) res = 0xC299;
    else if(addr >= 0xC283) res = 0xC283;
    else if(addr >= 0xC280) res = 0xC280;
    else if(addr >= 0xC26B) res = 0xC26B;
    else if(addr >= 0xC268) res = 0xC268;
    else if(addr >= 0xC260) res = 0xC260;
    else if(addr >= 0xC254) res = 0xC254;
    else if(addr >= 0xC24C) res = 0xC24C;
    else if(addr >= 0xC245) res = 0xC245;
    else if(addr >= 0xC228) res = 0xC228;
    else if(addr >= 0xC20A) res = 0xC20A;
    else if(addr >= 0xC200) res = 0xC200;
    else if(addr >= 0xC1F8) res = 0xC1F8;
    else if(addr >= 0xC1F3) res = 0xC1F3;
    else if(addr >= 0xC1EE) res = 0xC1EE;
    else if(addr >= 0xC1E5) res = 0xC1E5;
    else if(addr >= 0xC1D1) res = 0xC1D1;
    else if(addr >= 0xC1C8) res = 0xC1C8;
    else if(addr >= 0xC1BD) res = 0xC1BD;
    else if(addr >= 0xC1AD) res = 0xC1AD;
    else if(addr >= 0xC1A3) res = 0xC1A3;
    else if(addr >= 0xC194) res = 0xC194;
    else if(addr >= 0xC184) res = 0xC184;
    else if(addr >= 0xC17A) res = 0xC17A;
    else if(addr >= 0xC16A) res = 0xC16A;
    else if(addr >= 0xC160) res = 0xC160;
    else if(addr >= 0xC146) res = 0xC146;
    else if(addr >= 0xC12C) res = 0xC12C;
    else if(addr >= 0xC123) res = 0xC123;
    else if(addr >= 0xC118) res = 0xC118;
    else if(addr >= 0xC100) res = 0xC100;
    return res;
}

char* _get_c1541_routine_name(uint16_t addr) {
    switch (addr) {
        case 0xC100: return "SETLDA"; break;
        case 0xC118: return "LEDSON"; break;
        case 0xC123: return "ERROFF"; break;
        case 0xC12C: return "ERRON"; break;
        case 0xC146: return "PARSXQ"; break;
        case 0xC160: return "PS5"; break;
        case 0xC16A: return "PS10"; break;
        case 0xC17A: return "PS20"; break;
        case 0xC184: return "PS30"; break;
        case 0xC194: return "ENDCMD"; break;
        case 0xC1A3: return "SCREND"; break;
        case 0xC1AD: return "SCREN1"; break;
        case 0xC1BD: return "CLRCB"; break;
        case 0xC1C8: return "CMDERR"; break;
        case 0xC1D1: return "SIMPRS"; break;
        case 0xC1E5: return "PRSCLN"; break;
        case 0xC1EE: return "TAGCMD"; break;
        case 0xC1F3: return "TC25"; break;
        case 0xC1F8: return "TC30"; break;
        case 0xC200: return "TC35"; break;
        case 0xC20A: return "TC40"; break;
        case 0xC228: return "TC50"; break;
        case 0xC245: return "TC60"; break;
        case 0xC24C: return "TC70"; break;
        case 0xC254: return "TC75"; break;
        case 0xC260: return "TC80"; break;
        case 0xC268: return "PARSE"; break;
        case 0xC26B: return "PR10"; break;
        case 0xC280: return "PR20"; break;
        case 0xC283: return "PR25"; break;
        case 0xC299: return "PR28"; break;
        case 0xC29E: return "PR30"; break;
        case 0xC2A0: return "PR35"; break;
        case 0xC2B1: return "PR40"; break;
        case 0xC2B3: return "CMDSET"; break;
        case 0xC2CA: return "CS07"; break;
        case 0xC2CB: return "CS08"; break;
        case 0xC2DC: return "CMDRST"; break;
        case 0xC312: return "ONEDRV"; break;
        case 0xC320: return "ALLDRS"; break;
        case 0xC325: return "AD10"; break;
        case 0xC33C: return "SETDRV"; break;
        case 0xC34C: return "SD20"; break;
        case 0xC34D: return "SD22"; break;
        case 0xC34F: return "SD24"; break;
        case 0xC352: return "SD40"; break;
        case 0xC361: return "SD50"; break;
        case 0xC368: return "SETANY"; break;
        case 0xC370: return "SA05"; break;
        case 0xC383: return "SA10"; break;
        case 0xC388: return "SA20"; break;
        case 0xC38F: return "TOGDRV"; break;
        case 0xC398: return "FS1SET"; break;
        case 0xC3B0: return "FS10"; break;
        case 0xC3B8: return "FS15"; break;
        case 0xC3BD: return "TST0V1"; break;
        case 0xC3C7: return "T0V1"; break;
        case 0xC3CA: return "OPTSCH"; break;
        case 0xC3D5: return "OS10"; break;
        case 0xC3E8: return "OS15"; break;
        case 0xC3EF: return "OS30"; break;
        case 0xC400: return "OS35"; break;
        case 0xC41B: return "OS45"; break;
        case 0xC420: return "OS50"; break;
        case 0xC434: return "OS60"; break;
        case 0xC439: return "OS70"; break;
        case 0xC43C: return "OS45"; break;
        case 0xC440: return "SCHTBL"; break;
        case 0xC44F: return "LOOKUP"; break;
        case 0xC452: return "LK05"; break;
        case 0xC45C: return "LK10"; break;
        case 0xC462: return "LK15"; break;
        case 0xC470: return "LK20"; break;
        case 0xC475: return "LK25"; break;
        case 0xC47E: return "LK26"; break;
        case 0xC485: return "LK30"; break;
        case 0xC48B: return "FFRE"; break;
        case 0xC492: return "FF15"; break;
        case 0xC49D: return "FFST"; break;
        case 0xC4AA: return "FF10"; break;
        case 0xC4B5: return "FNDFIL"; break;
        case 0xC4BA: return "FF25"; break;
        case 0xC4C9: return "FF30"; break;
        case 0xC4D7: return "FF40"; break;
        case 0xC4D8: return "COMPAR"; break;
        case 0xC4E6: return "CP02"; break;
        case 0xC4E7: return "CP05"; break;
        case 0xC4EC: return "CP10"; break;
        case 0xC4FE: return "CP20"; break;
        case 0xC50A: return "CP30"; break;
        case 0xC51B: return "CP32"; break;
        case 0xC51D: return "CP33"; break;
        case 0xC52B: return "CP34"; break;
        case 0xC535: return "CP40"; break;
        case 0xC55C: return "CP42"; break;
        case 0xC589: return "CMPCHK"; break;
        case 0xC594: return "CC10"; break;
        case 0xC59A: return "CC15"; break;
        case 0xC5A6: return "CC20"; break;
        case 0xC5AC: return "SRCHST"; break;
        case 0xC5C4: return "SR10"; break;
        case 0xC5CA: return "SR15"; break;
        case 0xC5D7: return "SR20"; break;
        case 0xC5FB: return "SR30"; break;
        case 0xC604: return "SRRE"; break;
        case 0xC617: return "SEARCH"; break;
        case 0xC629: return "SR40"; break;
        case 0xC62F: return "SR50"; break;
        case 0xC63D: return "AUTOI"; break;
        case 0xC65F: return "AUTO1"; break;
        case 0xC669: return "AUTO2"; break;
        case 0xC66E: return "TRNAME"; break;
        case 0xC681: return "TN10"; break;
        case 0xC687: return "TN20"; break;
        case 0xC688: return "TRCMBF"; break;
        case 0xC697: return "TR10"; break;
        case 0xC6A5: return "TR20"; break;
        case 0xC6A6: return "FNDLMT"; break;
        case 0xC6AD: return "FL05"; break;
        case 0xC6C8: return "FL10"; break;
        case 0xC6CE: return "GETNAM"; break;
        case 0xC6DE: return "GNSUB"; break;
        case 0xC6F7: return "GN05"; break;
        case 0xC6FC: return "GN050"; break;
        case 0xC70E: return "GN051"; break;
        case 0xC71B: return "GN10"; break;
        case 0xC728: return "GN12"; break;
        case 0xC73C: return "GN14"; break;
        case 0xC74A: return "GN15"; break;
        case 0xC76B: return "GN20"; break;
        case 0xC773: return "GN22"; break;
        case 0xC783: return "GN30"; break;
        case 0xC7A7: return "GN40"; break;
        case 0xC7AB: return "GN45"; break;
        case 0xC7AC: return "BLKNB"; break;
        case 0xC7B0: return "BLKNB1"; break;
        case 0xC7B7: return "NEWDIR"; break;
        case 0xC7DC: return "ND10"; break;
        case 0xC7E5: return "ND15"; break;
        case 0xC7ED: return "ND20"; break;
        case 0xC806: return "MSGFRE"; break;
        case 0xC817: return "FREMSG"; break;
        case 0xC823: return "SCRTCH"; break;
        case 0xC835: return "SC15"; break;
        case 0xC855: return "SC17"; break;
        case 0xC86B: return "SC20"; break;
        case 0xC86D: return "SC25"; break;
        case 0xC872: return "SC30"; break;
        case 0xC87D: return "DELFIL"; break;
        case 0xC894: return "DEL2"; break;
        case 0xC8AD: return "DEL1"; break;
        case 0xC8B6: return "DELDIR"; break;
        case 0xC8C6: return "FORMAT"; break;
        case 0xC8E0: return "FMT105"; break;
        case 0xC8EF: return "FMT110"; break;
        case 0xC8F0: return "DSKCPY"; break;
        case 0xC90C: return "DX0000"; break;
        case 0xC90F: return "DX0005"; break;
        case 0xC923: return "DX0010"; break;
        case 0xC928: return "DX0020"; break;
        case 0xC932: return "PUPS1"; break;
        case 0xC952: return "COPY"; break;
        case 0xC982: return "COP01"; break;
        case 0xC987: return "COP05"; break;
        case 0xC9A1: return "COP10"; break;
        case 0xC9A7: return "CY"; break;
        case 0xC9B9: return "CY10"; break;
        case 0xC9CE: return "CY10A"; break;
        case 0xC9D5: return "CY15"; break;
        case 0xC9D8: return "CY20"; break;
        case 0xC9EA: return "CY30"; break;
        case 0xC9FA: return "OPIRFL"; break;
        case 0xCA31: return "OPIR10"; break;
        case 0xCA35: return "GIBYTE"; break;
        case 0xCA39: return "GCBYTE"; break;
        case 0xCA52: return "GIB20"; break;
        case 0xCA53: return "CYEXT"; break;
        case 0xCA88: return "RENAME"; break;
        case 0xCA97: return "RN10"; break;
        case 0xCACC: return "CHKIN"; break;
        case 0xCAD6: return "CK10"; break;
        case 0xCAE6: return "CK20"; break;
        case 0xCAE7: return "CHKIO"; break;
        case 0xCAEA: return "CK25"; break;
        case 0xCAF4: return "CK30"; break;
        case 0xCAF8: return "MEM"; break;
        case 0xCB1D: return "MEMEX"; break;
        case 0xCB20: return "MEMRD"; break;
        case 0xCB2B: return "MRMULT"; break;
        case 0xCB45: return "M30"; break;
        case 0xCB4B: return "MEMERR"; break;
        case 0xCB50: return "MEMWRT"; break;
        case 0xCB5C: return "USER"; break;
        case 0xCB63: return "USRINT"; break;
        case 0xCB6C: return "US10"; break;
        case 0xCB72: return "USREXC"; break;
        case 0xCB84: return "OPNBLK"; break;
        case 0xCBA0: return "OB05"; break;
        case 0xCBA5: return "OB10"; break;
        case 0xCBB8: return "OB15"; break;
        case 0xCBF1: return "OB30"; break;
        case 0xCC1B: return "BLOCK"; break;
        case 0xCC26: return "BLK10"; break;
        case 0xCC2B: return "BLK30"; break;
        case 0xCC30: return "BLK40"; break;
        case 0xCC38: return "BLK50"; break;
        case 0xCC42: return "BLK60"; break;
        case 0xCC6F: return "BLKPAR"; break;
        case 0xCC7C: return "BP05"; break;
        case 0xCC8B: return "BP10"; break;
        case 0xCC92: return "BP20"; break;
        case 0xCCA1: return "ASCHEX"; break;
        case 0xCCAB: return "AH10"; break;
        case 0xCCCA: return "AH20"; break;
        case 0xCCD0: return "AH30"; break;
        case 0xCCD7: return "AH35"; break;
        case 0xCCE4: return "AH40"; break;
        case 0xCCF2: return "DECTAB"; break;
        case 0xCCF5: return "BLKFRE"; break;
        case 0xCD03: return "BLKALC"; break;
        case 0xCD19: return "BA15"; break;
        case 0xCD1A: return "BA20"; break;
        case 0xCD2C: return "BA30"; break;
        case 0xCD31: return "BA40"; break;
        case 0xCD36: return "BLKRD2"; break;
        case 0xCD3C: return "GETSIM"; break;
        case 0xCD42: return "BLKRD3"; break;
        case 0xCD56: return "BLKRD"; break;
        case 0xCD5F: return "UBLKRD"; break;
        case 0xCD73: return "BLKWT"; break;
        case 0xCD81: return "BW10"; break;
        case 0xCD8C: return "BW20"; break;
        case 0xCD97: return "UBLKWT"; break;
        case 0xCDA3: return "BLKEXC"; break;
        case 0xCDBA: return "BE10"; break;
        case 0xCDBD: return "BLKPTR"; break;
        case 0xCDD2: return "BUFTST"; break;
        case 0xCDE0: return "BT15"; break;
        case 0xCDE5: return "BT20"; break;
        case 0xCDF2: return "BKOTST"; break;
        case 0xCDF5: return "BLKTST"; break;
        case 0xCE0E: return "FNDREL"; break;
        case 0xCE2C: return "MULPLY"; break;
        case 0xCE41: return "MUL25"; break;
        case 0xCE4C: return "MUL50"; break;
        case 0xCE50: return "MUL100"; break;
        case 0xCE57: return "MUL200"; break;
        case 0xCE6D: return "MUL400"; break;
        case 0xCE6E: return "DIV254"; break;
        case 0xCE71: return "DIV120"; break;
        case 0xCE87: return "DIV150"; break;
        case 0xCE89: return "DIV200"; break;
        case 0xCEA3: return "DIV300"; break;
        case 0xCEB0: return "DIV400"; break;
        case 0xCEBF: return "DIV500"; break;
        case 0xCED6: return "DIV600"; break;
        case 0xCED9: return "ZERRES"; break;
        case 0xCEE2: return "ACCX4"; break;
        case 0xCEE5: return "ACCX2"; break;
        case 0xCEE6: return "ACC200"; break;
        case 0xCEED: return "ADDRES"; break;
        case 0xCEF0: return "ADD100"; break;
        case 0xCEFA: return "LRUINT"; break;
        case 0xCEFC: return "LRUILP"; break;
        case 0xCF09: return "LRUUPD"; break;
        case 0xCF0D: return "LRULP1"; break;
        case 0xCF1D: return "LRUEXT"; break;
        case 0xCF1E: return "DBLBUF"; break;
        case 0xCF57: return "DBL05"; break;
        case 0xCF5D: return "DBL08"; break;
        case 0xCF66: return "DBL10"; break;
        case 0xCF6C: return "DBL15"; break;
        case 0xCF6F: return "DBL20"; break;
        case 0xCF76: return "DBL30"; break;
        case 0xCF7B: return "DBSET"; break;
        case 0xCF8B: return "DBS10"; break;
        case 0xCF8C: return "TGLBUF"; break;
        case 0xCF9B: return "PIBYTE"; break;
        case 0xCFAF: return "PBYTE"; break;
        case 0xCFB7: return "PUT"; break;
        case 0xCFBF: return "L40"; break;
        case 0xCFC9: return "L41"; break;
        case 0xCFCE: return "L46"; break;
        case 0xCFD8: return "L42"; break;
        case 0xCFE8: return "L50"; break;
        case 0xCFED: return "L45"; break;
        case 0xCFF1: return "PUTBYT"; break;
        case 0xCFFD: return "PUTB1"; break;
        case 0xD005: return "INTDRV"; break;
        case 0xD00B: return "ID20"; break;
        case 0xD00E: return "ITRIAL"; break;
        case 0xD024: return "IT20"; break;
        case 0xD02C: return "IT30"; break;
        case 0xD042: return "INITDR"; break;
        case 0xD075: return "NFCALC"; break;
        case 0xD07D: return "NUMF1"; break;
        case 0xD083: return "NUMF2"; break;
        case 0xD09B: return "STRRD"; break;
        case 0xD0AF: return "STRDBL"; break;
        case 0xD0B7: return "STR1"; break;
        case 0xD0C3: return "RDBUF"; break;
        case 0xD0C7: return "WRTBUF"; break;
        case 0xD0C9: return "STRTIT"; break;
        case 0xD0E8: return "WRTC1"; break;
        case 0xD0EB: return "FNDRCH"; break;
        case 0xD0F3: return "FNDC20"; break;
        case 0xD0F9: return "FNDC25"; break;
        case 0xD106: return "FNDC30"; break;
        case 0xD107: return "FNDWCH"; break;
        case 0xD10F: return "FNDW13"; break;
        case 0xD119: return "FNDW10"; break;
        case 0xD121: return "FNDW15"; break;
        case 0xD123: return "FNDW20"; break;
        case 0xD125: return "TYPFIL"; break;
        case 0xD12F: return "GETPRE"; break;
        case 0xD137: return "GETBYT"; break;
        case 0xD14D: return "GETB2"; break;
        case 0xD151: return "GETB1"; break;
        case 0xD156: return "RDBYT"; break;
        case 0xD164: return "RD01"; break;
        case 0xD16A: return "RD1"; break;
        case 0xD191: return "RD3"; break;
        case 0xD192: return "RD4"; break;
        case 0xD19D: return "WRTBYT"; break;
        case 0xD1A3: return "WRT0"; break;
        case 0xD1C6: return "INCPTR"; break;
        case 0xD1D3: return "SETDRN"; break;
        case 0xD1DF: return "GETWCH"; break;
        case 0xD1E2: return "GETRCH"; break;
        case 0xD1E3: return "GETR2"; break;
        case 0xD1F3: return "GETR52"; break;
        case 0xD1F5: return "GETR55"; break;
        case 0xD206: return "GETR3"; break;
        case 0xD20F: return "GETERR"; break;
        case 0xD217: return "GETR5"; break;
        case 0xD226: return "GETR4"; break;
        case 0xD227: return "FRECHN"; break;
        case 0xD22E: return "FRECO"; break;
        case 0xD249: return "RELINX"; break;
        case 0xD24D: return "REL15"; break;
        case 0xD253: return "REL10"; break;
        case 0xD259: return "FRE25"; break;
        case 0xD25A: return "RELBUF"; break;
        case 0xD26B: return "REL1"; break;
        case 0xD27C: return "REL2"; break;
        case 0xD28D: return "REL3"; break;
        case 0xD28E: return "GETBUF"; break;
        case 0xD2A3: return "GBF1"; break;
        case 0xD2B6: return "GBF2"; break;
        case 0xD2BA: return "FNDBUF"; break;
        case 0xD2BC: return "FB1"; break;
        case 0xD2C8: return "FB2"; break;
        case 0xD2D8: return "FB3"; break;
        case 0xD2D9: return "FRI20"; break;
        case 0xD2DA: return "FREIAC"; break;
        case 0xD2E9: return "FRI10"; break;
        case 0xD2F3: return "FREBUF"; break;
        case 0xD2F9: return "FREB1"; break;
        case 0xD307: return "CLRCHN"; break;
        case 0xD30B: return "CLRC1"; break;
        case 0xD313: return "CLDCHN"; break;
        case 0xD317: return "CLSD"; break;
        case 0xD339: return "STLBUF"; break;
        case 0xD33E: return "STL05"; break;
        case 0xD348: return "STL10"; break;
        case 0xD355: return "STL20"; break;
        case 0xD35E: return "STL30"; break;
        case 0xD363: return "STL40"; break;
        case 0xD373: return "STL50"; break;
        case 0xD37A: return "STL60"; break;
        case 0xD37F: return "FNDLDX"; break;
        case 0xD383: return "FND10"; break;
        case 0xD391: return "FND30"; break;
        case 0xD39B: return "GBYTE"; break;
        case 0xD3AA: return "GET"; break;
        case 0xD3B4: return "GET00"; break;
        case 0xD3CE: return "GET0"; break;
        case 0xD3D3: return "GET1"; break;
        case 0xD3D7: return "GET2"; break;
        case 0xD3DE: return "RNDGET"; break;
        case 0xD3EC: return "RNGET1"; break;
        case 0xD3EE: return "RNGET2"; break;
        case 0xD3F0: return "RNGET4"; break;
        case 0xD3FF: return "RNGET3"; break;
        case 0xD400: return "SEQGET"; break;
        case 0xD403: return "GET3"; break;
        case 0xD409: return "GET6"; break;
        case 0xD414: return "GETERC"; break;
        case 0xD433: return "GE10"; break;
        case 0xD43A: return "GE15"; break;
        case 0xD443: return "GE20"; break;
        case 0xD445: return "GE30"; break;
        case 0xD44D: return "NXTBUF"; break;
        case 0xD45F: return "NXTB1"; break;
        case 0xD460: return "DRTRD"; break;
        case 0xD464: return "DRTWRT"; break;
        case 0xD466: return "DRT"; break;
        case 0xD475: return "OPNIRD"; break;
        case 0xD477: return "OPNTYP"; break;
        case 0xD486: return "OPNIWR"; break;
        case 0xD48D: return "NXDRBK"; break;
        case 0xD4BB: return "NXDB1"; break;
        case 0xD4C8: return "SETPNT"; break;
        case 0xD4DA: return "FREICH"; break;
        case 0xD4E8: return "GETPNT"; break;
        case 0xD4EB: return "SETDIR"; break;
        case 0xD4F6: return "DRDBYT"; break;
        case 0xD506: return "SETLJB"; break;
        case 0xD50E: return "SETJOB"; break;
        case 0xD535: return "SJB2"; break;
        case 0xD538: return "SJB3"; break;
        case 0xD53F: return "SJB4"; break;
        case 0xD54A: return "TSERR"; break;
        case 0xD54D: return "TSER1"; break;
        case 0xD552: return "HED2TS"; break;
        case 0xD55F: return "TSCHK"; break;
        case 0xD572: return "VNERR"; break;
        case 0xD57A: return "SJB1"; break;
        case 0xD586: return "DOREAD"; break;
        case 0xD58A: return "DOWRIT"; break;
        case 0xD58C: return "DOJOB"; break;
        case 0xD590: return "DOIT"; break;
        case 0xD593: return "DOIT2"; break;
        case 0xD599: return "WATJOB"; break;
        case 0xD5A6: return "TSTJOB"; break;
        case 0xD5BA: return "TJ10"; break;
        case 0xD5C2: return "OK"; break;
        case 0xD5C4: return "NOTYET"; break;
        case 0xD5C6: return "RECOV"; break;
        case 0xD5E3: return "REC01"; break;
        case 0xD5F4: return "REC0"; break;
        case 0xD600: return "REC1"; break;
        case 0xD625: return "REC3"; break;
        case 0xD631: return "REC5"; break;
        case 0xD635: return "QUIT"; break;
        case 0xD63F: return "QUIT2"; break;
        case 0xD644: return "REC7"; break;
        case 0xD645: return "REC5"; break;
        case 0xD651: return "REC8"; break;
        case 0xD65C: return "REC9"; break;
        case 0xD66D: return "REC95"; break;
        case 0xD676: return "HEDOFF"; break;
        case 0xD67C: return "HOF1"; break;
        case 0xD688: return "HOF2"; break;
        case 0xD692: return "HOF3"; break;
        case 0xD693: return "MCVHED"; break;
        case 0xD69A: return "MH10"; break;
        case 0xD6A6: return "DOREC"; break;
        case 0xD6AB: return "DOREC1"; break;
        case 0xD6B9: return "DOREC2"; break;
        case 0xD6C4: return "DOREC3"; break;
        case 0xD6D0: return "SETHDR"; break;
        case 0xD6E4: return "ADDFIL"; break;
        case 0xD715: return "AF08"; break;
        case 0xD726: return "AF10"; break;
        case 0xD730: return "AF15"; break;
        case 0xD73D: return "AF20"; break;
        case 0xD74D: return "AF25"; break;
        case 0xD790: return "AF50"; break;
        case 0xD7B4: return "OPEN"; break;
        case 0xD7CF: return "OP02"; break;
        case 0xD7EB: return "ENDRD"; break;
        case 0xD7F3: return "OP021"; break;
        case 0xD7FF: return "OP04"; break;
        case 0xD815: return "OP041"; break;
        case 0xD81C: return "OP0415"; break;
        case 0xD82B: return "OP042"; break;
        case 0xD834: return "OP049"; break;
        case 0xD837: return "OP05"; break;
        case 0xD83C: return "OP10"; break;
        case 0xD840: return "OP20"; break;
        case 0xD876: return "OP40"; break;
        case 0xD891: return "OP45"; break;
        case 0xD8A7: return "OP50"; break;
        case 0xD8B1: return "OP60"; break;
        case 0xD8C6: return "OP75"; break;
        case 0xD8CD: return "OP77"; break;
        case 0xD8D9: return "OP80"; break;
        case 0xD8E1: return "OP81"; break;
        case 0xD8F0: return "OP815"; break;
        case 0xD8F5: return "OP82"; break;
        case 0xD940: return "OP90"; break;
        case 0xD945: return "OP95"; break;
        case 0xD94A: return "OP100"; break;
        case 0xD95C: return "OP110"; break;
        case 0xD965: return "OP115"; break;
        case 0xD96A: return "OP120"; break;
        case 0xD990: return "OP125"; break;
        case 0xD9A0: return "OPREAD"; break;
        case 0xD9C3: return "OP130"; break;
        case 0xD9E3: return "OPWRIT"; break;
        case 0xD9EF: return "OPFIN"; break;
        case 0xDA06: return "OPF1"; break;
        case 0xDA09: return "CKTM"; break;
        case 0xDA11: return "CKM1"; break;
        case 0xDA1C: return "CKM2"; break;
        case 0xDA29: return "CKT2"; break;
        case 0xDA2A: return "APPEND"; break;
        case 0xDA42: return "AP30"; break;
        case 0xDA55: return "LOADIR"; break;
        case 0xDA62: return "LD01"; break;
        case 0xDA6D: return "LD02"; break;
        case 0xDA86: return "LD03"; break;
        case 0xDA90: return "LD05"; break;
        case 0xDA9E: return "LD10"; break;
        case 0xDAA7: return "LD20"; break;
        case 0xDAC0: return "CLOSE"; break;
        case 0xDAD1: return "CLS05"; break;
        case 0xDAD4: return "CLS10"; break;
        case 0xDAE9: return "CLS15"; break;
        case 0xDAEC: return "CLSALL"; break;
        case 0xDAF0: return "CLS20"; break;
        case 0xDAFF: return "CLS25"; break;
        case 0xDB02: return "CLSCHN"; break;
        case 0xDB0C: return "CLSC28"; break;
        case 0xDB26: return "CLSC30"; break;
        case 0xDB29: return "CLSC31"; break;
        case 0xDB2C: return "CLSREL"; break;
        case 0xDB5F: return "CLSR1"; break;
        case 0xDB62: return "CLSWRT"; break;
        case 0xDB76: return "CLSW10"; break;
        case 0xDB88: return "CLSW15"; break;
        case 0xDB8C: return "CLSW20"; break;
        case 0xDBA5: return "CLSDIR"; break;
        case 0xDC06: return "CLSD4"; break;
        case 0xDC21: return "CLSD5"; break;
        case 0xDC29: return "CLSD6"; break;
        case 0xDC46: return "OPNRCH"; break;
        case 0xDC65: return "OR10"; break;
        case 0xDC81: return "OR20"; break;
        case 0xDC98: return "OROW"; break;
        case 0xDCA9: return "OR30"; break;
        case 0xDCB6: return "INITP"; break;
        case 0xDCDA: return "OPNWCH"; break;
        case 0xDCFD: return "OW10"; break;
        case 0xDD16: return "OW20"; break;
        case 0xDD8D: return "PUTSS"; break;
        case 0xDD95: return "SCFLG"; break;
        case 0xDD97: return "SETFLG"; break;
        case 0xDD9D: return "CLRFLG"; break;
        case 0xDDA3: return "CLRF10"; break;
        case 0xDDA6: return "TSTFLG"; break;
        case 0xDDAB: return "TSTWRT"; break;
        case 0xDDB7: return "TSTCHN"; break;
        case 0xDDB9: return "TSTC20"; break;
        case 0xDDC2: return "TSTC30"; break;
        case 0xDDC9: return "TSTRTS"; break;
        case 0xDDCA: return "TSTC40"; break;
        case 0xDDF1: return "SCRUB"; break;
        case 0xDDFC: return "SCR1"; break;
        case 0xDDFD: return "SETLNK"; break;
        case 0xDE0C: return "GETLNK"; break;
        case 0xDE19: return "NULLNK"; break;
        case 0xDE2B: return "SET00"; break;
        case 0xDE3B: return "CURBLK"; break;
        case 0xDE3E: return "GETHDR"; break;
        case 0xDE50: return "WRTAB"; break;
        case 0xDE57: return "RDAB"; break;
        case 0xDE5E: return "WRTOUT"; break;
        case 0xDE65: return "RDIN"; break;
        case 0xDE6C: return "WRTSS"; break;
        case 0xDE73: return "RDSS"; break;
        case 0xDE75: return "RDS5"; break;
        case 0xDE7F: return "SJ10"; break;
        case 0xDE8B: return "SJ20"; break;
        case 0xDE95: return "RDLNK"; break;
        case 0xDEA5: return "B0TOB0"; break;
        case 0xDEB6: return "B02"; break;
        case 0xDEC1: return "CLRBUF"; break;
        case 0xDECC: return "CB10"; break;
        case 0xDED2: return "SSSET"; break;
        case 0xDEDC: return "SSDIR"; break;
        case 0xDEE9: return "SETSSP"; break;
        case 0xDEF8: return "SSPOS"; break;
        case 0xDF0B: return "SSP10"; break;
        case 0xDF12: return "SSP20"; break;
        case 0xDF1B: return "IBRD"; break;
        case 0xDF21: return "IBWT"; break;
        case 0xDF25: return "IBOP"; break;
        case 0xDF45: return "GSSPNT"; break;
        case 0xDF4C: return "SCAL1"; break;
        case 0xDF51: return "SSCALC"; break;
        case 0xDF5C: return "ADDT12"; break;
        case 0xDF65: return "ADDRTS"; break;
        case 0xDF66: return "SSTEST"; break;
        case 0xDF77: return "ST10"; break;
        case 0xDF7B: return "ST20"; break;
        case 0xDF8B: return "ST30"; break;
        case 0xDF8F: return "ST40"; break;
        case 0xDF93: return "GETACT"; break;
        case 0xDF9B: return "GA1"; break;
        case 0xDF9E: return "GAFLGS"; break;
        case 0xDFA0: return "GA2"; break;
        case 0xDFB0: return "GA3"; break;
        case 0xDFB7: return "GETINA"; break;
        case 0xDFBF: return "GI10"; break;
        case 0xDFC2: return "PUTINA"; break;
        case 0xDFCD: return "PI1"; break;
        case 0xDFD0: return "NXTREC"; break;
        case 0xDFE4: return "NXTR15"; break;
        case 0xDFF6: return "NXTR20"; break;
        case 0xE009: return "NXOUT"; break;
        case 0xE018: return "NXTR45"; break;
        case 0xE01D: return "NXTR40"; break;
        case 0xE02A: return "NXTR50"; break;
        case 0xE034: return "NXTR30"; break;
        case 0xE035: return "NXTR35"; break;
        case 0xE03C: return "NRBUF"; break;
        case 0xE05D: return "NRBU50"; break;
        case 0xE06B: return "NRBU70"; break;
        case 0xE07B: return "NRBU20"; break;
        case 0xE07C: return "RELPUT"; break;
        case 0xE094: return "RELP06"; break;
        case 0xE096: return "RELP05"; break;
        case 0xE09E: return "RELP07"; break;
        case 0xE0A3: return "RELP10"; break;
        case 0xE0AA: return "RELP20"; break;
        case 0xE0AB: return "WRTREL"; break;
        case 0xE0B2: return "WR10"; break;
        case 0xE0B7: return "WR20"; break;
        case 0xE0BC: return "WR30"; break;
        case 0xE0C8: return "WR40"; break;
        case 0xE0D6: return "WR45"; break;
        case 0xE0D9: return "WR50"; break;
        case 0xE0E1: return "WR51"; break;
        case 0xE0E2: return "WR60"; break;
        case 0xE0F3: return "CLREC"; break;
        case 0xE104: return "CLR10"; break;
        case 0xE105: return "SDIRTY"; break;
        case 0xE115: return "CDIRTY"; break;
        case 0xE120: return "RDREL"; break;
        case 0xE127: return "RD10"; break;
        case 0xE138: return "RD15"; break;
        case 0xE13B: return "RD20"; break;
        case 0xE13D: return "RD25"; break;
        case 0xE14D: return "RD30"; break;
        case 0xE153: return "RD40"; break;
        case 0xE15E: return "RD05"; break;
        case 0xE16E: return "SETLST"; break;
        case 0xE17E: return "SETL01"; break;
        case 0xE19D: return "SETL05"; break;
        case 0xE1A4: return "SETL10"; break;
        case 0xE1AC: return "SETL40"; break;
        case 0xE1B2: return "FNDLST"; break;
        case 0xE1B7: return "FNDL10"; break;
        case 0xE1C4: return "FNDL30"; break;
        case 0xE1C8: return "FNDL20"; break;
        case 0xE1CE: return "SSEND"; break;
        case 0xE1D8: return "SE10"; break;
        case 0xE1DC: return "SE20"; break;
        case 0xE1EF: return "SE30"; break;
        case 0xE202: return "BREAK"; break;
        case 0xE207: return "RECORD"; break;
        case 0xE219: return "R20"; break;
        case 0xE228: return "R30"; break;
        case 0xE253: return "R40"; break;
        case 0xE265: return "R50"; break;
        case 0xE272: return "R60"; break;
        case 0xE275: return "POSITN"; break;
        case 0xE289: return "P2"; break;
        case 0xE291: return "P30"; break;
        case 0xE29C: return "POSBUF"; break;
        case 0xE2AA: return "P10"; break;
        case 0xE2BF: return "P75"; break;
        case 0xE2C2: return "P80"; break;
        case 0xE2D0: return "BHERE"; break;
        case 0xE2D3: return "BHERE2"; break;
        case 0xE2DC: return "BH10"; break;
        case 0xE2E2: return "NULBUF"; break;
        case 0xE2F1: return "NB20"; break;
        case 0xE303: return "NB30"; break;
        case 0xE304: return "ADDNR"; break;
        case 0xE318: return "AN05"; break;
        case 0xE31B: return "AN10"; break;
        case 0xE31C: return "ADDREL"; break;
        case 0xE33B: return "ADDR1"; break;
        case 0xE355: return "AR10"; break;
        case 0xE363: return "AR20"; break;
        case 0xE368: return "AR25"; break;
        case 0xE372: return "AR30"; break;
        case 0xE38F: return "AR35"; break;
        case 0xE39D: return "AR40"; break;
        case 0xE3B6: return "AR45"; break;
        case 0xE3C8: return "AR50"; break;
        case 0xE3D4: return "AR55"; break;
        case 0xE3F9: return "AR60"; break;
        case 0xE418: return "AR65"; break;
        case 0xE444: return "AR70"; break;
        case 0xE44E: return "NEWSS"; break;
        case 0xE4AC: return "NS20"; break;
        case 0xE4D1: return "NS40"; break;
        case 0xE4DE: return "NS50"; break;
        case 0xE60A: return "ERROR"; break;
        case 0xE625: return "ERR1"; break;
        case 0xE627: return "ERR2"; break;
        case 0xE62D: return "ERR3"; break;
        case 0xE644: return "ERR4"; break;
        case 0xE645: return "CMDER2"; break;
        case 0xE648: return "CMDER3"; break;
        case 0xE680: return "TLKERR"; break;
        case 0xE688: return "LSNERR"; break;
        case 0xE68E: return "TLERR"; break;
        case 0xE698: return "ERR10"; break;
        case 0xE69B: return "HEXDEC"; break;
        case 0xE69F: return "HEX0"; break;
        case 0xE6AA: return "HEX5"; break;
        case 0xE6AB: return "BCDDEC"; break;
        case 0xE6B4: return "BCD2"; break;
        case 0xE6BC: return "OKERR"; break;
        case 0xE6C1: return "ERRTSO"; break;
        case 0xE6C7: return "ERRMSG"; break;
        case 0xE706: return "ERMOVE"; break;
        case 0xE718: return "E10"; break;
        case 0xE722: return "E20"; break;
        case 0xE727: return "E30"; break;
        case 0xE735: return "E40"; break;
        case 0xE739: return "E45"; break;
        case 0xE73D: return "E50"; break;
        case 0xE742: return "E55"; break;
        case 0xE74D: return "E90"; break;
        case 0xE754: return "E60"; break;
        case 0xE763: return "E70"; break;
        case 0xE767: return "EADV1"; break;
        case 0xE76D: return "EA10"; break;
        case 0xE775: return "EADV2"; break;
        case 0xE77E: return "EA20"; break;
        case 0xE77F: return "BOOT2"; break;
        case 0xE780: return "BOOT"; break;
        case 0xE78E: return "BOOT3"; break;
        case 0xE7A3: return "UTLODR"; break;
        case 0xE7A8: return "BOOT4"; break;
        case 0xE7C5: return "UTLD00"; break;
        case 0xE7D8: return "UTLD10"; break;
        case 0xE7FA: return "UTLD20"; break;
        case 0xE802: return "UTLD30"; break;
        case 0xE817: return "UTLD35"; break;
        case 0xE82C: return "UTLD50"; break;
        case 0xE839: return "GTABYT"; break;
        case 0xE848: return "GTABYE"; break;
        case 0xE84B: return "ADDSUM"; break;
        case 0xE853: return "ATNIRQ"; break;
        case 0xE85B: return "ATNSRV"; break;
        case 0xE87B: return "ATNS15"; break;
        case 0xE891: return "ATN35"; break;
        case 0xE89B: return "ATN40"; break;
        case 0xE8A9: return "ATN45"; break;
        case 0xE8B7: return "ATN50"; break;
        case 0xE8D2: return "ATN95"; break;
        case 0xE8D7: return "ATSN20"; break;
        case 0xE8ED: return "ATN100"; break;
        case 0xE8FA: return "ATN110"; break;
        case 0xE8FD: return "ATN120"; break;
        case 0xE902: return "ATN122"; break;
        case 0xE909: return "TALK"; break;
        case 0xE90F: return "TALK1"; break;
        case 0xE915: return "NOTLK"; break;
        case 0xE916: return "TLK05"; break;
        case 0xE925: return "TALK2"; break;
        case 0xE937: return "TLK02"; break;
        case 0xE941: return "TLK03"; break;
        case 0xE94B: return "NOEOI"; break;
        case 0xE95C: return "ISR01"; break;
        case 0xE963: return "ISR02"; break;
        case 0xE973: return "ISRHI"; break;
        case 0xE976: return "ISRCLK"; break;
        case 0xE980: return "ISR03"; break;
        case 0xE987: return "ISR04"; break;
        case 0xE999: return "FRMERX"; break;
        case 0xE99C: return "DATHI"; break;
        case 0xE9A5: return "DATLOW"; break;
        case 0xE9AE: return "CLKLOW"; break;
        case 0xE9B7: return "CLKHI"; break;
        case 0xE9C0: return "DEBNC"; break;
        case 0xE9C9: return "ACPTR"; break;
        case 0xE9CD: return "ACP00A"; break;
        case 0xE9DF: return "ACP00"; break;
        case 0xE9F2: return "ACPOOB"; break;
        case 0xE9FD: return "ACP02A"; break;
        case 0xEA07: return "ACP01"; break;
        case 0xEA0B: return "ACP03"; break;
        case 0xEA1A: return "ACP03A"; break;
        case 0xEA2E: return "LISTEN"; break;
        case 0xEA39: return "LSN15"; break;
        case 0xEA44: return "LSN30"; break;
        case 0xEA4E: return "FRMERR"; break;
        case 0xEA56: return "ATNLOW"; break;
        case 0xEA59: return "TSTATN"; break;
        case 0xEA62: return "TSTRTN"; break;
        case 0xEA63: return "TSTA50"; break;
        case 0xEA6B: return "TATN20"; break;
        case 0xEA6E: return "PEZRO"; break;
        case 0xEA71: return "PERR"; break;
        case 0xEA74: return "PE20"; break;
        case 0xEA75: return "PE30"; break;
        case 0xEA7D: return "REA7D"; break;
        case 0xEA7E: return "PD10"; break;
        case 0xEA7F: return "PD20"; break;
        case 0xEA8E: return "PE40"; break;
        case 0xEA8F: return "PD11"; break;
        case 0xEA90: return "PD21"; break;
        case 0xEAA0: return "DSKINT"; break;
        case 0xEAAC: return "PV10"; break;
        case 0xEAB2: return "PV20"; break;
        case 0xEAB7: return "PV30"; break;
        case 0xEAC9: return "RM10"; break;
        case 0xEAD5: return "RT10"; break;
        case 0xEAD7: return "RT20"; break;
        case 0xEAEA: return "CR20"; break;
        case 0xEAEC: return "CR30"; break;
        case 0xEAF0: return "RAMTST"; break;
        case 0xEAF2: return "RA10"; break;
        case 0xEB02: return "RA30"; break;
        case 0xEB04: return "RA40"; break;
        case 0xEB1F: return "PERR2"; break;
        case 0xEB22: return "DIAGOK"; break;
        case 0xEB4B: return "INTTAB"; break;
        case 0xEB4F: return "INTT1"; break;
        case 0xEB76: return "DSKIN1"; break;
        case 0xEB7E: return "DSKIN2"; break;
        case 0xEBD5: return "SETERR"; break;
        case 0xEBE7: return "IDLE"; break;
        case 0xEBFF: return "IDL1"; break;
        case 0xEC07: return "IDL01"; break;
        case 0xEC12: return "IDL02"; break;
        case 0xEC2B: return "IDL3"; break;
        case 0xEC31: return "IDL4"; break;
        case 0xEC3B: return "IDL5"; break;
        case 0xEC58: return "IDL6"; break;
        case 0xEC5C: return "IDL7"; break;
        case 0xEC69: return "IDL8"; break;
        case 0xEC6D: return "IDL9"; break;
        case 0xEC81: return "IDL10"; break;
        case 0xEC8B: return "IDL11"; break;
        case 0xEC98: return "IDL12"; break;
        case 0xEC9E: return "STDIR"; break;
        case 0xECEA: return "DIR1"; break;
        case 0xED0D: return "DIR10"; break;
        case 0xED23: return "DIR3"; break;
        case 0xED59: return "MOVBUF"; break;
        case 0xED5B: return "MOVB1"; break;
        case 0xED67: return "GETDIR"; break;
        case 0xED6D: return "GETD3"; break;
        case 0xED7E: return "GD1"; break;
        case 0xED84: return "VERDIR"; break;
        case 0xED9C: return "VD10"; break;
        case 0xEDB3: return "VD15"; break;
        case 0xEDCB: return "VD17"; break;
        case 0xEDD4: return "VD20"; break;
        case 0xEDD9: return "VD25"; break;
        case 0xEDE5: return "VMKBAM"; break;
        case 0xEDEE: return "MRK2"; break;
        case 0xEE04: return "MRK1"; break;
        case 0xEE0D: return "NEW"; break;
        case 0xEE19: return "N101"; break;
        case 0xEE46: return "N108"; break;
        case 0xEE56: return "N110"; break;
        case 0xEEB7: return "NEWMAP"; break;
        case 0xEEC7: return "NM10"; break;
        case 0xEED9: return "NM20"; break;
        case 0xEEE3: return "NM30"; break;
        case 0xEEF4: return "MAPOUT"; break;
        case 0xEEFF: return "SCRBAM"; break;
        case 0xEF07: return "SB10"; break;
        case 0xEF24: return "SB20"; break;
        case 0xEF3A: return "SETBPT"; break;
        case 0xEF4D: return "NUMFRE"; break;
        case 0xEF5C: return "WFREE"; break;
        case 0xEF5F: return "FRETS"; break;
        case 0xEF62: return "FRETS2"; break;
        case 0xEF87: return "FRERTS"; break;
        case 0xEF88: return "DTYBAM"; break;
        case 0xEF90: return "WUSED"; break;
        case 0xEF93: return "USEDTS"; break;
        case 0xEFBA: return "USE10"; break;
        case 0xEFBD: return "USE20"; break;
        case 0xEFCE: return "USERTS"; break;
        case 0xEFCF: return "FREUSE"; break;
        case 0xEFD3: return "FREUS2"; break;
        case 0xEFD5: return "FREUS3"; break;
        case 0xEFE9: return "BMASK"; break;
        case 0xEFF1: return "FIXBAM"; break;
        case 0xF004: return "FBAM10"; break;
        case 0xF005: return "CLRBAM"; break;
        case 0xF00B: return "CLB1"; break;
        case 0xF011: return "SETBAM"; break;
        case 0xF022: return "SBM10"; break;
        case 0xF03E: return "SBM30"; break;
        case 0xF05B: return "SWAP"; break;
        case 0xF07F: return "SWAP3"; break;
        case 0xF09F: return "SWAP4"; break;
        case 0xF0A5: return "PUTBAM"; break;
        case 0xF0BE: return "SWAP1"; break;
        case 0xF0D0: return "SWAP2"; break;
        case 0xF0D1: return "CLNBAM"; break;
        case 0xF0DF: return "REDBAM"; break;
        case 0xF0F2: return "RBM10"; break;
        case 0xF10A: return "RBM20"; break;
        case 0xF10F: return "BAM2A"; break;
        case 0xF118: return "B2X10"; break;
        case 0xF119: return "BAAM2X"; break;
        case 0xF11E: return "NXTTS"; break;
        case 0xF12D: return "NXTDS"; break;
        case 0xF130: return "NXT1"; break;
        case 0xF15A: return "NXTERR"; break;
        case 0xF15F: return "NXT2"; break;
        case 0xF173: return "FNDNXT"; break;
        case 0xF195: return "FNDN0"; break;
        case 0xF19A: return "FNDN1"; break;
        case 0xF19D: return "FNDN2"; break;
        case 0xF1A9: return "INTTS"; break;
        case 0xF1CB: return "ITS2"; break;
        case 0xF1DF: return "ITS3"; break;
        case 0xF1E6: return "FNDSEC"; break;
        case 0xF1F5: return "DERR"; break;
        case 0xF1FA: return "GETSEC"; break;
        case 0xF20D: return "GS10"; break;
        case 0xF21D: return "GS20"; break;
        case 0xF21F: return "GS30"; break;
        case 0xF220: return "AVCK"; break;
        case 0xF22B: return "AC10"; break;
        case 0xF22D: return "AC20"; break;
        case 0xF236: return "AC30"; break;
        case 0xF246: return "AC40"; break;
        case 0xF24B: return "MAXSEC"; break;
        case 0xF24E: return "MAX1"; break;
        case 0xF258: return "KILLP"; break;
        case 0xF259: return "CNTINT"; break;
        case 0xF2B0: return "LCC"; break;
        case 0xF2BE: return "TOP"; break;
        case 0xF2C3: return "CONT10"; break;
        case 0xF2CD: return "CONT30"; break;
        case 0xF2D8: return "CONT35"; break;
        case 0xF2E9: return "CONT40"; break;
        case 0xF2F3: return "CONT20"; break;
        case 0xF2F9: return "QUE"; break;
        case 0xF306: return "QUE05"; break;
        case 0xF320: return "QUE20"; break;
        case 0xF33C: return "GOTU"; break;
        case 0xF367: return "EXE"; break;
        case 0xF377: return "EX"; break;
        case 0xF37C: return "BMP"; break;
        case 0xF393: return "SETJB"; break;
        case 0xF3B1: return "SEAK"; break;
        case 0xF3C8: return "SEEK15"; break;
        case 0xF3D8: return "SEEK30"; break;
        case 0xF407: return "SEEK20"; break;
        case 0xF410: return "ESEEK"; break;
        case 0xF418: return "DONE"; break;
        case 0xF41B: return "BADID"; break;
        case 0xF41E: return "CSERR"; break;
        case 0xF423: return "WSECT"; break;
        case 0xF432: return "L460"; break;
        case 0xF43A: return "L480"; break;
        case 0xF461: return "L465"; break;
        case 0xF473: return "DOITT"; break;
        case 0xF47E: return "TSTRDJ"; break;
        case 0xF483: return "L470"; break;
        case 0xF497: return "CNVRTN"; break;
        case 0xF4CA: return "READ"; break;
        case 0xF4D1: return "READ01"; break;
        case 0xF4D4: return "READ11"; break;
        case 0xF4DF: return "READ20"; break;
        case 0xF4FB: return "READ28"; break;
        case 0xF50A: return "DSTRT"; break;
        case 0xF510: return "SRCH"; break;
        case 0xF538: return "SRCH20"; break;
        case 0xF53D: return "SRCH25"; break;
        case 0xF54E: return "SRCH30"; break;
        case 0xF553: return "ERR"; break;
        case 0xF556: return "SYNC"; break;
        case 0xF55D: return "SYNC10"; break;
        case 0xF56E: return "WRIGHT"; break;
        case 0xF586: return "WRT10"; break;
        case 0xF5AB: return "WRTSNC"; break;
        case 0xF5B3: return "WRT30"; break;
        case 0xF5BF: return "WRT40"; break;
        case 0xF5E9: return "CHKBLK"; break;
        case 0xF5F2: return "WTOBIN"; break;
        case 0xF624: return "WTOB14"; break;
        case 0xF643: return "WTOB50"; break;
        case 0xF64F: return "WTOB53"; break;
        case 0xF66E: return "WTOB52"; break;
        case 0xF683: return "WTOB57"; break;
        case 0xF691: return "VRFY"; break;
        case 0xF6A3: return "VRF15"; break;
        case 0xF6B3: return "VRF30"; break;
        case 0xF6C5: return "VRF20"; break;
        case 0xF6CA: return "SECTSK"; break;
        case 0xF6D0: return "PUT4GB"; break;
        case 0xF77F: return "BGTAB"; break;
        case 0xF78F: return "BINGCR"; break;
        case 0xF7BA: return "BING07"; break;
        case 0xF7E6: return "GET4GB"; break;
        case 0xF8E0: return "GCRBIN"; break;
        case 0xF90C: return "GCRB10"; break;
        case 0xF92B: return "GCRB20"; break;
        case 0xF934: return "CONHDR"; break;
        case 0xF969: return "ERRR"; break;
        case 0xF975: return "ERRR10"; break;
        case 0xF97E: return "TURNON"; break;
        case 0xF98F: return "TRNOFF"; break;
        case 0xF99C: return "END"; break;
        case 0xF9D6: return "END33X"; break;
        case 0xF9D9: return "END10"; break;
        case 0xF9E4: return "END20"; break;
        case 0xF9FA: return "END30"; break;
        case 0xFA05: return "INACT"; break;
        case 0xFA0E: return "INAC10"; break;
        case 0xFA1C: return "INAC20"; break;
        case 0xFA2E: return "DOSTEP"; break;
        case 0xFA32: return "STPOUT"; break;
        case 0xFA3B: return "SHORT"; break;
        case 0xFA4E: return "SETLE"; break;
        case 0xFA63: return "STPIN"; break;
        case 0xFA69: return "STP"; break;
        case 0xFA7B: return "SSACL"; break;
        case 0xFA94: return "SSA10"; break;
        case 0xFA97: return "SSRUN"; break;
        case 0xFAA5: return "SSDEC"; break;
        case 0xFABE: return "END33"; break;
        case 0xFAC7: return "FORMT"; break;
        case 0xFAF5: return "L213"; break;
        case 0xFB00: return "L214"; break;
        case 0xFB0C: return "TOPP"; break;
        case 0xFB39: return "FWAIT"; break;
        case 0xFB3E: return "FWAIT2"; break;
        case 0xFB43: return "F000"; break;
        case 0xFB46: return "F001"; break;
        case 0xFB5C: return "F005"; break;
        case 0xFB64: return "F006"; break;
        case 0xFB67: return "F007"; break;
        case 0xFB7D: return "F009"; break;
        case 0xFBB6: return "COUNT"; break;
        case 0xFBBB: return "CNT10"; break;
        case 0xFBCE: return "CNT20"; break;
        case 0xFBE0: return "DS08"; break;
        case 0xFC3F: return "MAK10"; break;
        case 0xFC86: return "CRTDAT"; break;
        case 0xFCB1: return "WRTSYN"; break;
        case 0xFCB8: return "WRTS10"; break;
        case 0xFCC2: return "WRTS20"; break;
        case 0xFCD1: return "WRTS30"; break;
        case 0xFCE0: return "DBSYNC"; break;
        case 0xFCEB: return "WRTS40"; break;
        case 0xFCF9: return "WRTS50"; break;
        case 0xFD09: return "WGP2"; break;
        case 0xFD2C: return "COMP"; break;
        case 0xFD39: return "CMPR10"; break;
        case 0xFD40: return "CMPR15"; break;
        case 0xFD58: return "CMPR20"; break;
        case 0xFD62: return "TSTDAT"; break;
        case 0xFD67: return "TST05"; break;
        case 0xFD77: return "TST10"; break;
        case 0xFD96: return "FMTEND"; break;
        case 0xFDA3: return "SYNCLR"; break;
        case 0xFDB9: return "SYC10"; break;
        case 0xFDC3: return "WRTNUM"; break;
        case 0xFDC9: return "WRTN10"; break;
        case 0xFDD3: return "FMTERR"; break;
        case 0xFDDB: return "FMTE10"; break;
        case 0xFDE5: return "MOVUP"; break;
        case 0xFDF5: return "MOVOVR"; break;
        case 0xFE00: return "KILL"; break;
        case 0xFE0E: return "CLEAR"; break;
        case 0xFE26: return "CLER10"; break;
        case 0xFE30: return "FBTOG"; break;
        case 0xFE44: return "FBG10"; break;
        case 0xFE67: return "SYSIRQ"; break;
        case 0xFE76: return "IRQ10"; break;
        case 0xFE7F: return "IRQ20"; break;
        case 0xFEE7: return "NMI"; break;
        case 0xFEEA: return "PEA7A"; break;
        case 0xFEF3: return "SLOWD"; break;
        case 0xFF01: return "NNMI"; break;
        case 0xFF0D: return "NNMI10"; break;
        default:
            return NULL;
    }
}

#ifdef __IEC_DEBUG
static void _c1541_debug_out_processor_pc(c1541_t* sys, uint64_t pins) {
    if (pins & M6502_SYNC) {
        uint16_t cpu_pc = m6502_pc(&sys->cpu);
        uint16_t function_address = _get_c1541_nearest_routine_address(cpu_pc);
        uint16_t address_diff = cpu_pc - function_address;
        char* function_name = _get_c1541_routine_name(function_address);
        if (function_name == NULL) {
            function_name = "";
        }
        if (sys->debug_file) {
            char iec_status[5];
            char local_iec_status[5];
            iec_get_status_text(sys->iec_bus, iec_status);
            iec_get_device_status_text(sys->iec_device, local_iec_status);

            uint8_t iec_lines = iec_get_signals(sys->iec_bus, sys->iec_device);
            uint64_t via1_pins = pins & M6502_PIN_MASK;
            via1_pins &= ~(M6522_PB0 | M6522_PB2 | M6522_PB7 | M6522_CA1);
            if (iec_lines & IECLINE_ATN) {
                via1_pins |= M6522_PB7;
                via1_pins |= M6522_CA1;
            }
            if (iec_lines & IECLINE_CLK) {
                via1_pins |= M6522_PB2;
            }
            if (iec_lines & IECLINE_DATA /*|| ((via1_pb & (1<<7)) >> 3) ^ (via1_pb & (1<<4))*/) {
                via1_pins |= M6522_PB0;
            }
            via1_pins = (via1_pins >> M6522_PIN_PB0) & 0xff;

            if (address_diff > 0) {
                fprintf(sys->debug_file, "tick:%10ld\taddr:%04x\tsys:c1541\tport-via1-b:%02x\tbus-iec:%s\tlocal-iec:%s\tlabel:%s+%x\n", get_world_tick(), cpu_pc, via1_pins, iec_status, local_iec_status, function_name, address_diff);
            } else {
                fprintf(sys->debug_file, "tick:%10ld\taddr:%04x\tsys:c1541\tport-via1-b:%02x\tbus-iec:%s\tlocal-iec:%s\tlabel:%s\n", get_world_tick(), cpu_pc, via1_pins, iec_status, local_iec_status, function_name);
            }

            if(cpu_pc==0xf4da) {
              if ((cpu_pc & 0xC000) == 0xC000) { // ROM
                _show_debug_trace('8', &sys->cpu, get_world_tick(),
                                  sys->rom[(cpu_pc+0) & 0x3FFF], sys->rom[(cpu_pc+1) & 0x3FFF], sys->rom[(cpu_pc+2) & 0x3FFF]);
              } else if (cpu_pc < 0x0800) { // RAM
                _show_debug_trace('8', &sys->cpu, get_world_tick(),
                                  sys->ram[(cpu_pc+0) & 0x07FF], sys->ram[(cpu_pc+1) & 0x07FF], sys->ram[(cpu_pc+2) & 0x07FF]);
              }
            }
        }
    }
}
#endif

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

uint64_t _c1541_tick(c1541_t* sys, uint64_t pins) {
#ifdef __IEC_DEBUG
    _c1541_debug_out_processor_pc(sys, pins);
#endif
    iec_get_signals(sys->iec_bus, NULL);
    
    // FIXME: this code only exists for debugging purpose
    if (sys->exit_countdown == 1) {
        printf("\n\n*** sys->exit_countdown == 1 - c1541.h debug exit***\n");
        exit(0);
    } else if (sys->exit_countdown > 0) {
        sys->exit_countdown--;
    }

    // FIXME: the start address of an instruction is only memorized for debugging purpose
    uint16_t cpu_addr = M6502_GET_ADDR(pins);
    bool is_cpu_sync = (pins & M6502_SYNC);
    if (is_cpu_sync) {
        _1541_last_cpu_address = cpu_addr;
    }

    // s0 pin high workaround for injecting OV flag to the cpu on a new instruction
    if (is_cpu_sync && sys->byte_ready && (sys->via_2.pins & M6522_CA2)) {
        m6502_set_p(&sys->cpu, m6502_p(&sys->cpu)|M6502_VF);
    }
    
    pins = m6502_tick(&sys->cpu, pins);
    const uint16_t addr = M6502_GET_ADDR(pins);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(M6502_IRQ);

    uint64_t via1_pins = pins & M6502_PIN_MASK;
    uint64_t via2_pins = pins & M6502_PIN_MASK;

    if (pins & M6502_RW) {
        // memory read
        bool valid_read = true;
        uint8_t read_data = 0;
        if ((addr & 0xC000) == 0xC000) {
            // Read from ROM
            read_data = sys->rom[addr & 0x3FFF];
        } else if ((addr & 0xFC00) == 0x1800) {
            // Read from VIA1
            read_data = _m6522_read(&sys->via_1, addr & 0xF);
	    // FIXME: debugging purpose
            if (addr == 0x1800) {
                printf("%ld - 1541 - Read VIA1 $1800 = $%02X - CPU @ $%04X\n", get_world_tick(), read_data, _1541_last_cpu_address);
            }
        } else if ((addr & 0xFC00) == 0x1C00) {
            // Read from VIA2
            read_data = _m6522_read(&sys->via_2, addr & 0xF);
        } else if (addr < 0x0800) {
            // Read from RAM
            read_data = sys->ram[addr & 0x7FF];
        } else {
            // Illegal access
            printf("Illegal read! $%04x\n", addr);
            valid_read = false;
        }
        if (valid_read) {
            M6502_SET_DATA(pins, read_data);
        }
    }
    else {
        // memory write
        uint8_t write_data = M6502_GET_DATA(pins);
        // FIXME: debugging purpose
        if (addr == 0x1800) {
            printf("%ld - 1541 - Write VIA1 $1800 = $%02X - CPU @ $%04X\n", get_world_tick(), write_data, _1541_last_cpu_address);
        }
        _c1541_write(sys, addr, write_data);
    }

    if ((addr & 0xFC00) == 0x1800) {
        via1_pins |= M6522_CS1;
    } else if ((addr & 0xFC00) == 0x1C00) {
        via2_pins |= M6522_CS1;
    }

    // tick VIA1
    {
        uint8_t iec_lines = iec_get_signals(sys->iec_bus, sys->iec_device);

        via1_pins &= ~(M6522_PB0 | M6522_PB2 | M6522_PB7 | M6522_CA1);
        if (iec_lines & IECLINE_ATN) {
            via1_pins |= M6522_PB7;
            via1_pins |= M6522_CA1;
        }
        if (iec_lines & IECLINE_CLK) {
            via1_pins |= M6522_PB2;
        }
        if (sys->via_1.pins & M6522_PB3) {
            via1_pins |= M6522_PB2;
        }
        if (iec_lines & IECLINE_DATA) {
            via1_pins |= M6522_PB0;
        }
        if (XOR(sys->via_1.pins & M6522_PB4, iec_lines & IECLINE_ATN)) {
            // ATNA logic (UD3) DATA read back. Note setting of ATN is done in _c1541_write_iec_pins()
            via1_pins |= M6522_PB0;
        }
        if (sys->via_1.pins & M6522_PB1) {
            via1_pins |= M6522_PB0;
        }

        via1_pins = m6522_tick(&sys->via_1, via1_pins);
        
        if (via1_pins & M6502_IRQ) {
            pins |= M6502_IRQ;
        }
        
        _c1541_write_iec_pins(sys, via1_pins);
    }
    

    bool was_byte_ready = sys->byte_ready;
    bool via_output = false;
    {
        // Prepare VIA2 pins

        bool output_enable = (sys->via_2.pins & M6522_CB2) != 0;
        bool motor_active = (sys->via_2.pins & M6522_PB2) != 0;
        bool is_sync = (((sys->current_data + 1) & (1<<10)) != 0) && output_enable;
        bool drive_ready = (sys->ram[0x20] & 0x80) == 0;
        bool drive_on = (sys->ram[0x20] & 0x30) == 0x20;
        bool drive_stepping = (sys->ram[0x20] & 0x60) == 0x60;

        // Motor active?
        if (motor_active && drive_on) {
            //// FIXME: for debugging purpose the countdown runs for 2 simulated seconds and terminates the program afterward
            //if (sys->exit_countdown == 0) {
            //    sys->exit_countdown = C1541_FREQUENCY << 1;
            //}
            via_output = true;
            sys->hundred_cycles_rotor_active += 100; // fixed point arithmetic - use 2 digits as fractional part
            if (sys->hundred_cycles_rotor_active >= sys->hundred_cycles_per_bit) {
                sys->hundred_cycles_rotor_active -= sys->hundred_cycles_per_bit;
                
                // we are going to read new bits, so deactivate byte_ready
                sys->byte_ready = false;

                // shift in next gcr bit
                sys->current_data <<= 1;
                sys->current_data &= (1<<10)-1;
                if (sys->gcr_bytes[sys->gcr_byte_pos] & (1<<(7-sys->gcr_bit_pos))) {
                    // GCR 1 bit
                    sys->current_data |= 1;
                } else {
                    // GCR 0 bit
                }

                // update gcr read position
                sys->gcr_bit_pos++;
                if (sys->gcr_bit_pos > 7) {
                    sys->gcr_bit_pos = 0;
                    sys->gcr_byte_pos++;
                    if (sys->gcr_byte_pos >= sys->gcr_size || sys->gcr_bytes[sys->gcr_byte_pos] == 0) {
                        sys->gcr_byte_pos = 0;
			// FIXME: debug output
                        printf("%ld - 1541 - disk cycle complete\n", get_world_tick());
                    }
                }

                is_sync = (((sys->current_data + 1) & (1<<10)) != 0) && output_enable;
                
                if (is_sync) {
                    sys->output_bit_counter = 0;
                } else {
                    sys->output_bit_counter++;
                    if (sys->output_bit_counter > 7) {
                        sys->output_bit_counter = 0;
                        sys->byte_ready = true;
                    }
                }
            }
        } else {
            sys->byte_ready = false;
        }

        if (!is_sync) {
            via2_pins |= M6522_PB7;
        } else {
            sys->byte_ready = false;
        }

        if (sys->byte_ready) {
            sys->output_data = sys->current_data & 0xff;
            if (output_enable) {
                via2_pins |= M6522_CA1;
            }
        }

        if (output_enable) {
            M6522_SET_PA(via2_pins, sys->output_data);
        }
    }

    // tick VIA2
    {
        via2_pins = m6522_tick(&sys->via_2, via2_pins);
        if (via2_pins & M6502_IRQ) {
            pins |= M6502_IRQ;
        }
    }

    return pins;
}

void c1541_tick(c1541_t* sys) {
    sys->pins = _c1541_tick(sys, sys->pins);
    return;
}

void c1541_insert_disc(c1541_t* sys, chips_range_t data) {
    // FIXME
    (void)sys;
    (void)data;
}

void c1541_remove_disc(c1541_t* sys) {
    // FIXME
    (void)sys;
}

void c1541_snapshot_onsave(c1541_t* snapshot, void* base) {
    CHIPS_ASSERT(snapshot && base);
    m6502_snapshot_onsave(&snapshot->cpu);
    mem_snapshot_onsave(&snapshot->mem, base);
}

void c1541_snapshot_onload(c1541_t* snapshot, c1541_t* sys, void* base) {
    CHIPS_ASSERT(snapshot && sys && base);
    m6502_snapshot_onload(&snapshot->cpu, &sys->cpu);
    mem_snapshot_onload(&snapshot->mem, base);
}

#endif // CHIPS_IMPL
