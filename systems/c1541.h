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

#ifdef __cplusplus
extern "C" {
#endif

// IEC port bits, same as C64_IECPORT_*
#define C1541_IECPORT_RESET (1<<0)
#define C1541_IECPORT_SRQIN (1<<1)
#define C1541_IECPORT_DATA  (1<<2)
#define C1541_IECPORT_CLK   (1<<3)
#define C1541_IECPORT_ATN   (1<<4)

#define C1541_FREQUENCY (1000000)

// config params for c1541_init()
typedef struct {
    // pointer to a shared byte with IEC serial bus line state
    uint8_t* iec_port;
    // rom images
    struct {
        chips_range_t c000_dfff;
        chips_range_t e000_ffff;
    } roms;
} c1541_desc_t;

// 1541 emulator state
typedef struct {
    uint64_t pins;
    uint8_t* iec_port;
    m6502_t cpu;
    m6522_t via_1;
    m6522_t via_2;
    bool valid;
    mem_t mem;
    uint8_t ram[0x0800];
    uint8_t rom[0x4000];
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

void c1541_init(c1541_t* sys, const c1541_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);

    memset(sys, 0, sizeof(c1541_t));
    sys->valid = true;

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

    // setup memory map
    mem_init(&sys->mem);
    mem_map_ram(&sys->mem, 0, 0x0000, 0x0800, sys->ram);
    mem_map_rom(&sys->mem, 0, 0xC000, 0x4000, sys->rom);

    sys->iec_port = desc->iec_port;
}

void c1541_discard(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void c1541_reset(c1541_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->pins |= M6502_RES;
    m6522_reset(&sys->via_1);
    m6522_reset(&sys->via_2);
}

// Update the data lines based on the current address and read/write state
uint8_t _c1541_read(c1541_t* sys, uint16_t addr) {
    uint8_t res = 0;
    char *area = "n/a";
    if ((addr & 0xC000) == 0xC000) {
        // Read from ROM
        area = "rom";
        res = sys->rom[addr & 0x3FFF];
    } else if ((addr & 0xFC00) == 0x1800) {
        // Read from VIA1
        area = "via1";
        res = _m6522_read(&sys->via_1, addr & 0xF);
    } else if ((addr & 0xFC00) == 0x1C00) {
        // Read from VIA2
        area = "via2";
        res = _m6522_read(&sys->via_2, addr & 0xF);
    } else if (addr < 0x0800) {
        // Read from RAM
        area = "ram";
        res = sys->ram[addr & 0x7FF];
    } else {
        // Illegal access
        area = "illegal";
    }
    if (0) {
        printf("1541-read %s $%x = $%x\n", area, addr, res);
    }
    return res;
}

void _c1541_write(c1541_t* sys, uint16_t addr, uint8_t data) {
    char *area = "n/a";
    if ((addr & 0xFC00) == 0x1800) {
        area = "via1";
        _m6522_write(&sys->via_1, addr & 0xF, data);
    } else if ((addr & 0xFC00) == 0x1C00) {
        // Write to VIA2
        area = "via2";
        _m6522_write(&sys->via_2, addr & 0xF, data);
    } else if (addr < 0x0800) {
        // Write to RAM
        area = "ram";
        sys->ram[addr & 0x7FF] = data;
    } else {
        // Illegal access
        area = "illegal";
    }
    if (0) {
        printf("1541-write %s $%x $%x\n", area, addr, data);
    }
}

void _c1541_write_iec(c1541_t* sys) {
    if (sys->via_2.pins & M6522_PB4) {
        *sys->iec_port |= C1541_IECPORT_ATN;
    } else {
        *sys->iec_port &= ~C1541_IECPORT_ATN;
    }
    if (sys->via_2.pins & M6522_PB3) {
        *sys->iec_port |= C1541_IECPORT_CLK;
    } else {
        *sys->iec_port &= ~C1541_IECPORT_CLK;
    }
    if (sys->via_2.pins & M6522_PB1) {
        *sys->iec_port |= C1541_IECPORT_DATA;
    } else {
        *sys->iec_port &= ~C1541_IECPORT_DATA;
    }
}

void _c1541_read_iec(c1541_t* sys) {
    sys->via_2.pins &= ~(M6522_CA1 | M6522_PB7 | M6522_PB2 | M6522_PB0);
    if (*sys->iec_port & C1541_IECPORT_ATN) {
        sys->via_2.pins |= M6522_CA1 | M6522_PB7;
    }
    if (*sys->iec_port & C1541_IECPORT_CLK) {
        sys->via_2.pins |= M6522_PB2;
    }
    if (*sys->iec_port & C1541_IECPORT_DATA) {
        sys->via_2.pins |= M6522_PB0;
    }
    if (*sys->iec_port & C1541_IECPORT_RESET) {
        c1541_reset(sys);
    }
}

uint8_t last_via1_0 = 255;
uint8_t last_via1_1 = 255;
void debug_out_via1_status(uint8_t data0, uint8_t data1) {
    uint8_t motor_position = data0 & 0x3;
    uint8_t motor_on = (data0 & 0x04) >> 2;
    uint8_t drive_led = (data0 & 0x08) >> 3;
    uint8_t write_protect = (data0 & 0x10) >> 4;
    uint8_t density = (data0 & 0x60) >> 5;
    uint8_t sync_detect = (data0 & 0x80) >> 7;

    if (data0 != last_via1_0 || data1 != last_via1_1) {
        last_via1_0 = data0;
        last_via1_1 = data1;
        printf("VIA1");
        printf("\tLED: ");
        if (drive_led) {
            printf("on");
        } else {
            printf("off");
        }
        printf("\tMotor: ");
        if (motor_on) {
            printf("on (%d)", motor_position);
        } else {
            printf("off");
        }
        if (write_protect) {
            printf("\tRO");
        } else {
            printf("\tRW");
        }
        printf("\tDensity: %d", density);
        printf("\tSync: %d", sync_detect);
        printf("\tGCR: $%02x", data1);
        printf("\n");
    }
}

uint8_t last_via2_0 = 255;
uint8_t last_via2_1 = 255;
void debug_out_via2_status(uint8_t data0, uint8_t data1) {
    uint8_t data_in = (data0 & 0x1);
    uint8_t data_out = (data0 & 0x2) >> 1;
    uint8_t clock_in = (data0 & 0x04) >> 2;
    uint8_t clock_out = (data0 & 0x08) >> 3;
    uint8_t atn_in = (data0 & 0x80) >> 7;
    uint8_t atn_ack = (data0 & 0x10) >> 4;

    if (data0 != last_via2_0 || data1 != last_via2_1) {
        last_via2_0 = data0;
        last_via2_1 = data1;
        printf("VIA2");
        printf("\tDATA_IN: %d", data_in);
        printf("\tDATA_OUT: %d", data_out);
        printf("\tCLOCK_IN: %d", clock_in);
        printf("\tCLOCK_OUT: %d", clock_out);
        printf("\tATN_IN: %d", atn_in);
        printf("\tATN_ACK: %d", atn_ack);
        printf("\tunused: $%x", data1);
        printf("\n");
    }
}

void c1541_tick(c1541_t* sys) {
    // Apply IEC signals to VIA2 pins
    _c1541_read_iec(sys);

    // Tick VIA chips
    sys->via_1.pins = m6522_tick(&sys->via_1, sys->via_1.pins);
    sys->via_2.pins = m6522_tick(&sys->via_2, sys->via_2.pins);

    _c1541_write_iec(sys);
    
    // debug_out_via1_status(_c1541_read(sys, 0x1c00), _c1541_read(sys, 0x1c01));
    // debug_out_via2_status(_c1541_read(sys, 0x1800), _c1541_read(sys, 0x1801));

    // printf("\n");

    // Handle IRQs
    if ((sys->via_1.pins & M6522_IRQ) || (sys->via_2.pins & M6522_IRQ)) {
        sys->pins |= M6502_IRQ;
    } else {
        sys->pins &= ~M6502_IRQ;
    }

    // Tick the CPU
    sys->pins = m6502_tick(&sys->cpu, sys->pins);

    // Handle memory access based on CPU pins
    uint16_t addr = M6502_GET_ADDR(sys->pins);
    if (sys->pins & M6502_RW) {
        // CPU Read
        M6502_SET_DATA(sys->pins, _c1541_read(sys, addr));
    } else {
        // CPU Write
        _c1541_write(sys, addr, M6502_GET_DATA(sys->pins));
    }
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
    snapshot->iec_port = 0;
    m6502_snapshot_onsave(&snapshot->cpu);
    mem_snapshot_onsave(&snapshot->mem, base);
}

void c1541_snapshot_onload(c1541_t* snapshot, c1541_t* sys, void* base) {
    CHIPS_ASSERT(snapshot && sys && base);
    snapshot->iec_port = sys->iec_port;
    m6502_snapshot_onload(&snapshot->cpu, &sys->cpu);
    mem_snapshot_onload(&snapshot->mem, base);
}

#endif // CHIPS_IMPL
