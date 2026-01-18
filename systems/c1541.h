#pragma once
/*#
    # c1541.h

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

    You need to include the following headers before including c1541.h:

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
#include "disk_helpers.h"
// #include "disass.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C1541_FREQUENCY (1000000)

#ifndef C1541_LED_CHANGED_HOOK
#define C1541_LED_CHANGED_HOOK(s,b)
#endif
#ifndef C1541_MOTOR_CHANGED_HOOK
#define C1541_MOTOR_CHANGED_HOOK(s,b)
#endif
#ifndef C1541_TRACK_CHANGED_HOOK
#define C1541_TRACK_CHANGED_HOOK(s,v)
#endif

// Convert full track number (1-42) to half-track number (1, 3, 5, ...)
static inline uint8_t c1541_full_track_to_half_track(uint8_t full_track) {
    return (full_track << 1) - 1; // 1 -> 1, 2 -> 3, 3 -> 5, ...
}

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

#ifdef HAVE_CONNOMORE_M6502H
#define C1541_GET_ADDR(pins,sys) (sys->cpu.bus_addr)
#define C1541_GET_DATA(pins,sys) (sys->cpu.bus_data)
#define C1541_SET_DATA(pins,sys,data) sys->cpu.bus_data=data
#else
#define C1541_GET_ADDR(pins,sys) M6502_GET_ADDR(pins)
#define C1541_GET_DATA(pins,sys) M6502_GET_DATA(pins)
#define C1541_SET_DATA(pins,sys,data) M6502_SET_DATA(pins,data)
#endif

// config params for c1541_init()
typedef struct {
    // the IEC bus to connect to
    iecbus_t* iec_bus;
    // rom images
    struct {
        chips_range_t c000_dfff;
        chips_range_t e000_ffff;
    } roms;
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
    uint32_t rotor_nanoseconds_counter;
    uint32_t nanoseconds_per_bit;
    bool rotor_active;
    uint8_t gcr_bytes[0x2000];
    uint32_t gcr_size;
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

    char disk_filename[256];
    bool disk_loaded;
    uint8_t disk_type;  // 0=none, 1=G64, 2=D64
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
// attach disk image file (quick validation only, stores filename)
bool c1541_attach_disk(c1541_t* sys, const char* filename);
// fetch track data for current half-track position (opens file, reads offsets, reads track)
bool c1541_fetch_track(c1541_t* sys);

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

const uint32_t c1541_speedzone[] = { 4000, 3750, 3500, 3250 }; // nanoseconds per bit

void c1541_init(c1541_t* sys, const c1541_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);

    uint8_t initial_full_track = 1;

    memset(sys, 0, sizeof(c1541_t));
    sys->valid = true;
    // Initialize as empty drive
    sys->disk_filename[0] = '\0';
    sys->disk_loaded = false;
    sys->disk_type = 0;
    sys->gcr_size = 0;
    sys->gcr_bytes[0] = 0;
    sys->gcr_byte_pos = 0;
    sys->gcr_bit_pos = 0;
    sys->current_byte = 0;
    sys->current_bit_pos = 0;
    sys->nanoseconds_per_bit = c1541_speedzone[1];
    sys->rotor_nanoseconds_counter = 0;
    sys->half_track = c1541_full_track_to_half_track(initial_full_track);

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

    // use iec_bus instance if we got passed one
    if(desc->iec_bus) {
      sys->iec_bus = desc->iec_bus;
      CHIPS_ASSERT(sys->iec_bus);
    }

    // this will create an iec_bus instance if we don't have one yet
    sys->iec_device = iec_connect(&sys->iec_bus);
    CHIPS_ASSERT(sys->iec_device);
}

void c1541_discard(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    c1541_remove_disc(sys);
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

// static uint16_t _1541_last_cpu_address = 0;
// uint8_t last_via2_0_write = 0xff;
// uint8_t last_via2_1_write = 0xff;
// uint8_t last_via2_2_write = 0xff;
// uint8_t last_via2_3_write = 0xff;
void _c1541_write(c1541_t* sys, uint16_t addr, uint8_t data) {
    char *area = "n/a";
    bool illegal = false;
//    bool changed = false;

    if ((addr & 0xFC00) == 0x1800) {
        area = "via1";
        _m6522_write(&sys->via_1, addr & 0xF, data);
    } else if ((addr & 0xFC00) == 0x1C00) {
        // Write to VIA2
        area = "via2";
        if (addr == 0x1C00) {
            sys->rotor_active = (data & VIA2_ROTOR) != 0;
            if (!sys->rotor_active) {
                sys->rotor_nanoseconds_counter = 0;
            }
            sys->nanoseconds_per_bit = c1541_speedzone[data & 3];
        }
        // if ((addr & 0x0f) == 0 && (data != last_via2_0_write)) {
        //     last_via2_0_write = data;
        //     //changed = true;
        // }
        // if ((addr & 0x0f) == 1 && (data != last_via2_1_write)) {
        //     last_via2_1_write = data;
        //     //changed = true;
        // }
        // if ((addr & 0x0f) == 2 && (data != last_via2_2_write)) {
        //     last_via2_2_write = data;
        //     //changed = true;
        // }
        // if ((addr & 0x0f) == 3 && (data != last_via2_3_write)) {
        //     last_via2_3_write = data;
        //     //changed = true;
        // }
//	// FIXME: for debugging purpose only
//        if (changed) {
//            printf("%ld - 1541 - VIA2 Write $%04X = $%02X - CPU @ $%04X - Stepper=(%d%d[%d%d]) Rotor=(%d[%d]) LED=(%d[%d]) R/O=(%d[%d]) BitRate=(%d%d[%d%d]) Sync=(%d[%d])\n",
//                get_world_tick(),
//                addr,
//                data,
//                _1541_last_cpu_address,
//                last_via2_0_write & (1<<1) ? 1 : 0,
//                last_via2_0_write & (1<<0) ? 1 : 0,
//                last_via2_2_write & (1<<1) ? 1 : 0,
//                last_via2_2_write & (1<<0) ? 1 : 0,
//                last_via2_0_write & (1<<2) ? 1 : 0,
//                last_via2_2_write & (1<<2) ? 1 : 0,
//                last_via2_0_write & (1<<3) ? 1 : 0,
//                last_via2_2_write & (1<<3) ? 1 : 0,
//                last_via2_0_write & (1<<4) ? 1 : 0,
//                last_via2_2_write & (1<<4) ? 1 : 0,
//                last_via2_0_write & (1<<6) ? 1 : 0,
//                last_via2_0_write & (1<<5) ? 1 : 0,
//                last_via2_2_write & (1<<6) ? 1 : 0,
//                last_via2_2_write & (1<<5) ? 1 : 0,
//                last_via2_0_write & (1<<7) ? 1 : 0,
//                last_via2_2_write & (1<<7) ? 1 : 0
//            );
//        }
        if((addr & 0xF) == 0) {
            // $1c00 write
            uint8_t changed_bits = sys->via_2.pb.outr ^ data;
            if(changed_bits & 0b1000) {
                C1541_LED_CHANGED_HOOK(sys, !!(data & 0b1000));
            }
            if(changed_bits & 0b100) {
                C1541_MOTOR_CHANGED_HOOK(sys, !!(data & 0b100));
            }
        }
        _m6522_write(&sys->via_2, addr & 0xF, data);
    } else if (addr < 0x0800) {
        // Write to RAM
        // area = "ram";
        sys->ram[addr & 0x7FF] = data;
    } else {
        // Illegal access
        area = "illegal";
        illegal = true;
    }
    if (illegal) {
        printf("1541-write illegal %s $%x $%x\n", area, addr, data);
    }
}

#define XOR(a,b) (((a) && !(b)) || ((b) && !(a)))
#define AND(a,b) ((a) && (b))

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

uint64_t _c1541_tick_cpu(c1541_t *sys, const uint64_t input_pins) {
#ifdef PICO
    uint32_t tick = get_ticks();
#endif
    const bool is_cpu_sync = (input_pins & M6502_SYNC);

    // s0 pin high workaround for injecting OV flag to the cpu on a new instruction
    if (is_cpu_sync && sys->byte_ready && (sys->via_2.pins & M6522_CA2)) {
        m6502_set_p(&sys->cpu, m6502_p(&sys->cpu)|M6502_VF);
    }

// #ifdef PICO
//     uint32_t tick_cpu = get_ticks();
// #endif
    // printf("cpu pc: $%04x\n", sys->cpu.PC);
#ifdef PICO
    uint32_t chip_tick = get_ticks();
#endif
    uint64_t pins = m6502_tick(&sys->cpu, input_pins);
#ifdef PICO
    ticks_chip_cpu = get_elapsed_ticks(chip_tick);
#endif
// #ifdef PICO
//     uint32_t dt_cpu = get_elapsed_ticks(tick_cpu);
//     printf("chip_tick cpu: %ld sys tick(s)\n", dt_cpu);
// #endif

    const uint16_t addr = C1541_GET_ADDR(pins, sys);

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
            //	    // FIXME: debugging purpose
            //            if (addr == 0x1800) {
            //                printf("%ld - 1541 - Read VIA1 $1800 = $%02X - CPU @ $%04X\n", get_world_tick(), read_data, _1541_last_cpu_address);
            //            }
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
            C1541_SET_DATA(pins, sys, read_data);
        }
    }
    else {
        // memory write
        //         uint8_t write_data = C1541_GET_DATA(pins, sys);
        // //        // FIXME: debugging purpose
        // //        if (addr == 0x1800) {
        // //            printf("%ld - 1541 - Write VIA1 $1800 = $%02X - CPU @ $%04X\n", get_world_tick(), write_data, _1541_last_cpu_address);
        // //        }
        _c1541_write(sys, addr, C1541_GET_DATA(pins, sys));
    }

#ifdef PICO
    ticks_cpu = get_elapsed_ticks(tick);
#endif
    return pins;
}

// _c1541_tick_via1 returns if IRQ should be set
uint8_t _c1541_tick_via1(c1541_t* sys) {
#ifdef PICO
    uint32_t tick = get_ticks();
#endif
    uint64_t pins = sys->via_1.pins;

    // 1. "Tick" the IEC bus (reflects back active outputs).
    uint8_t iec_lines = iec_get_signals(sys->iec_bus);

    // 2. Write IEC signals to VIA inputs.
    pins &= ~(M6522_PB0 | M6522_PB2 | M6522_PB7 | M6522_CA1);
    if (IEC_ATN_ACTIVE(iec_lines)) {
        pins |= M6522_PB7; // ATN IN
        pins |= M6522_CA1;
    }
    if (IEC_CLK_ACTIVE(iec_lines)) {
        pins |= M6522_PB2; // CLK IN
    }
    if (IEC_DATA_ACTIVE(iec_lines)) {
        pins |= M6522_PB0; // DATA IN
    }

    // 3. Tick VIA1
    #ifdef PICO
    uint32_t chip_tick = get_ticks();
    pins = m6522_tick(&sys->via_1, pins);
    ticks_chip_via1 = get_elapsed_ticks(chip_tick);
    #else
    pins = m6522_tick(&sys->via_1, pins);
    #endif

    // 4. Write VIA outputs to IEC bus.
    uint8_t out_signals = ~0;
    if (pins & M6522_PB3) {
        out_signals &= ~IECLINE_CLK;
    }
    if (pins & M6522_PB1) {
        out_signals &= ~IECLINE_DATA;
    } else {
        // ATNA logic (UD3) may set DATA out, too
        if (XOR(IEC_ATN_ACTIVE(iec_lines), pins & M6522_PB4)) {
            // printf("ATNA activates DATA ATN(%d) ATNA(%d)\n", !!(iec_lines & IECLINE_ATN), !!(pins & M6522_PB4));
            out_signals &= ~IECLINE_DATA;
        }
    }
    iec_set_signals(sys->iec_bus, sys->iec_device, out_signals);

#ifdef PICO
    ticks_via1 = get_elapsed_ticks(tick);
#endif
    return 0 != (pins & M6522_IRQ);
}

// _c1541_tick_via2 returns if IRQ should be set
uint8_t _c1541_tick_via2(c1541_t* sys) {
#ifdef PICO
    uint32_t tick = get_ticks();
#endif
    uint64_t pins = sys->via_2.pins;
    {
        // Prepare VIA2 pins
// #ifdef PICO
//         uint32_t tick = get_ticks();
// #endif

        bool output_enable = (sys->via_2.pins & M6522_CB2) != 0;
        bool motor_active = (sys->via_2.pins & M6522_PB2) != 0;
        bool sync_bits_present = ((sys->current_data + 1) & (1<<10)) != 0;
        bool is_sync = sync_bits_present && output_enable;
//        bool drive_ready = (sys->ram[0x20] & 0x80) == 0;
        bool drive_on = (sys->ram[0x20] & 0x30) == 0x20;

        // Motor active?
        if (motor_active && drive_on) {
            //// FIXME: for debugging purpose the countdown runs for 2 simulated seconds and terminates the program afterward
            //if (sys->exit_countdown == 0) {
            //    sys->exit_countdown = C1541_FREQUENCY << 1;
            //}
//            via_output = true;
            sys->rotor_nanoseconds_counter += 1000;
            if (sys->rotor_nanoseconds_counter >= sys->nanoseconds_per_bit) {
                sys->rotor_nanoseconds_counter -= sys->nanoseconds_per_bit;
                
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
                        //printf("%ld - 1541 - disk cycle complete\n", get_world_tick());
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

        pins &= ~M6522_PB7;
        if (!is_sync) {
            pins |= M6522_PB7;
        } else {
            sys->byte_ready = false;
        }

        pins &= ~M6522_CA1;
        if (sys->byte_ready) {
            sys->output_data = sys->current_data & 0xff;
            if (output_enable) {
                pins |= M6522_CA1;
            }
        }

        if (output_enable) {
            M6522_SET_PA(pins, sys->output_data);
        }
// #ifdef PICO
//         uint32_t dt = get_elapsed_ticks(tick);
//         printf("chip_tick via2 prepare: %ld sys tick(s)\n", dt);
// #endif
    }

    // tick VIA2
    {
// #ifdef PICO
//         uint32_t tick = get_ticks();
// #endif
#ifdef PICO
        uint32_t chip_tick = get_ticks();
#endif
        pins = _m6522_tick(&sys->via_2, pins); // use internal _m6522_tick(), register reads/writes are done in cpu tick directly
        sys->via_2.pins = pins;
#ifdef PICO
        ticks_chip_via2 = get_elapsed_ticks(chip_tick);
#endif
// #ifdef PICO
//         uint32_t dt = get_elapsed_ticks(tick);
//         printf("chip_tick via2: %ld sys tick(s)\n", dt);
// #endif
        // if (pins & M6502_IRQ) {
        //     pins |= M6502_IRQ;
        // }
        // bool drive_stepping = (sys->ram[0x20] & 0x60) == 0x60;
        int stepper_position = (pins >> M6522_PIN_PB0) & 3;
        static int last_position = 0;
        if (last_position != stepper_position) {
            if ((stepper_position - last_position) == -1 || (stepper_position + 4 - last_position) == -1) {
                if (sys->half_track > 1) {
                    sys->half_track--;
                }
                //printf("Moving inward, track: %g\n", ((float)sys->half_track + 1) / 2);
            } else {
                if (sys->half_track < 42 * 2) {
                    sys->half_track++;
                }
                //printf("Moving outward, track: %g\n", ((float)sys->half_track + 1) / 2);
            }
            C1541_TRACK_CHANGED_HOOK(sys, sys->half_track);
            c1541_fetch_track(sys); //TODO do this in background/only after the head settled
            last_position = stepper_position;
        }
    }
#ifdef PICO
    ticks_via2 = get_elapsed_ticks(tick);
#endif
    return 0 != (pins & M6522_IRQ);
}

uint64_t _c1541_tick(c1541_t* sys, const uint64_t input_pins) {
    uint64_t pins = 0;
    const uint64_t via_pin_mask = M6502_PIN_MASK;

    pins = _c1541_tick_cpu(sys, input_pins); // VIA1/2 reg reads/writes are handled here, too
    pins &= ~(M6502_IRQ);

    if (_c1541_tick_via1(sys)) {
        pins |= M6502_IRQ;
    }

    if (_c1541_tick_via2(sys)) {
        pins |= M6502_IRQ;
    }

    return pins;
}

void
#ifdef PICO
__not_in_flash_func(c1541_tick)
#else
c1541_tick
#endif
(c1541_t* sys) {
    sys->pins = _c1541_tick(sys, sys->pins);
}

void c1541_insert_disc(c1541_t* sys, chips_range_t data) {
    // FIXME
    (void)sys;
    (void)data;
}

bool c1541_attach_disk(c1541_t* sys, const char* filename) {
    CHIPS_ASSERT(sys && sys->valid);
    CHIPS_ASSERT(filename);

    c1541_remove_disc(sys);

    // Check file extension
    const char* ext = strrchr(filename, '.');
    bool is_d64 = (ext && strcmp(ext, ".d64") == 0);

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("c1541: failed to open disk image: %s\n", filename);
        return false;
    }

    if (is_d64) {
        // D64: check file size (minimum 35-track D64 = 683 sectors * 256 bytes)
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fclose(fp);

        if (file_size < 683 * 256) {
            printf("c1541: file too small for D64: %s\n", filename);
            return false;
        }

        sys->disk_type = 2;  // D64
    } else {
        // G64: verify signature
        uint8_t signature[9];
        if (fread(signature, 1, 9, fp) != 9) {
            fclose(fp);
            return false;
        }
        fclose(fp);

        const uint8_t* expected = (uint8_t*)"GCR-1541";
        for (int i = 0; i < 8; i++) {
            if (signature[i] != expected[i]) {
                printf("c1541: invalid G64 format: %s\n", filename);
                return false;
            }
        }

        sys->disk_type = 1;  // G64
    }

    // Store filename
    strncpy(sys->disk_filename, filename, sizeof(sys->disk_filename) - 1);
    sys->disk_filename[sizeof(sys->disk_filename) - 1] = '\0';
    sys->disk_loaded = true;

    printf("c1541: attached disk image: %s (%s)\n", filename,
           is_d64 ? "D64" : "G64");
    return true;
}

bool c1541_fetch_track(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    if (!sys->disk_loaded || sys->disk_filename[0] == '\0') {
        sys->gcr_size = 0;
        sys->gcr_bytes[0] = 0;
        return false;
    }

    // D64 handling: convert sectors to GCR on-demand
    if (sys->disk_type == 2) {
        // Get full track number from half-track
        uint8_t full_track = (sys->half_track + 1) / 2;
        if (full_track < 1 || full_track > MAX_TRACKS_1541) {
            return false;
        }

        // Only process odd half-tracks (actual tracks)
        if (sys->half_track % 2 == 0) {
            // Even half-tracks have no data in D64
            sys->gcr_size = 0;
            sys->gcr_bytes[0] = 0;
            return true;
        }

        // Open D64 file
        FILE* fp = fopen(sys->disk_filename, "rb");
        if (!fp) {
            return false;
        }

        // Read disk ID from BAM (track 18, sector 0, offset 0xA2)
        // Track 18 starts after tracks 1-17: 17 tracks * 21 sectors * 256 bytes = 91728
        // Disk ID is at offset 0xA2 within sector 0: 91728 + 162 = 91890
        uint8_t disk_id[2] = {'0', '0'};  // Default
        uint32_t bam_offset = d64_track_offset(18) + 0xA2;
        if (fseek(fp, bam_offset, SEEK_SET) == 0) {
            if (fread(disk_id, 1, 2, fp) == 2) {
                // Successfully read disk ID
            }
        }

        // Seek to track start
        uint32_t track_offset = d64_track_offset(full_track);
        if (fseek(fp, track_offset, SEEK_SET) != 0) {
            fclose(fp);
            return false;
        }

        // Read and convert sectors to GCR
        uint8_t *ptr = sys->gcr_bytes;
        uint8_t sector_buffer[256];
        uint16_t sector_size = SYNC_LENGTH + HEADER_LENGTH + HEADER_GAP_LENGTH +
                               SYNC_LENGTH + DATA_LENGTH;

        uint8_t num_sectors = sector_map[full_track];
        for (uint8_t sector = 0; sector < num_sectors; sector++) {
            if (fread(sector_buffer, 1, 256, fp) != 256) {
                fclose(fp);
                return false;
            }

            convert_sector_to_GCR(sector_buffer, ptr, full_track, sector, disk_id);
            ptr += sector_size + sector_gap_length[full_track];
        }

        fclose(fp);

        // Calculate actual data written and expected track capacity
        uint16_t actual_size = (uint16_t)(ptr - sys->gcr_bytes);
        uint8_t speed_zone = speed_map[full_track];
        uint16_t expected_size = track_capacity[speed_zone];

        // Pad remaining space with gap bytes (0x55) to match expected track size
        if (actual_size < expected_size && actual_size < sizeof(sys->gcr_bytes)) {
            memset(ptr, 0x55, expected_size - actual_size);
            actual_size = expected_size;
        }

        if (actual_size > sizeof(sys->gcr_bytes) - 1) {
            actual_size = sizeof(sys->gcr_bytes) - 1;
        }
        sys->gcr_size = actual_size;
        sys->gcr_bytes[sys->gcr_size] = 0;  // Mark end of track
        return true;
    }

    // G64 handling (existing code)
    FILE* fp = fopen(sys->disk_filename, "rb");
    if (!fp) {
        return false;
    }

    // Read header
    uint8_t header[12];
    if (fread(header, 1, 12, fp) != 12) {
        fclose(fp);
        return false;
    }

    uint8_t half_track_count = header[9];
    if (sys->half_track < 1 || sys->half_track > half_track_count) {
        fclose(fp);
        return false;
    }

    // Read track offset for this half-track
    uint32_t track_offset;
    if (fseek(fp, 0xc + (sys->half_track - 1) * 4, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }
    if (fread(&track_offset, 4, 1, fp) != 1) {
        fclose(fp);
        return false;
    }

    if (track_offset == 0) {
        // Empty track
        sys->gcr_size = 0;
        sys->gcr_bytes[0] = 0;
        fclose(fp);
        return true;
    }

    // Seek to track data
    if (fseek(fp, track_offset, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    // Read track size (2 bytes, little-endian)
    uint8_t size_bytes[2];
    if (fread(size_bytes, 1, 2, fp) != 2) {
        fclose(fp);
        return false;
    }
    uint16_t data_size = size_bytes[0] | (size_bytes[1] << 8);

    if (data_size > sizeof(sys->gcr_bytes)) {
        data_size = sizeof(sys->gcr_bytes);
    }

    // Read track data
    if (fread(sys->gcr_bytes, 1, data_size, fp) != data_size) {
        fclose(fp);
        return false;
    }

    fclose(fp);

    sys->gcr_size = data_size;
    sys->gcr_bytes[data_size] = 0;  // Mark end of track

    return true;
}

void c1541_remove_disc(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    sys->disk_filename[0] = '\0';
    sys->disk_loaded = false;
    sys->disk_type = 0;
    sys->gcr_size = 0;
    sys->gcr_bytes[0] = 0;
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
