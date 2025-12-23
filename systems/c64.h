#pragma once
/*#
    # 64.h

    An Commodore C64 (PAL) emulator in a C header

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
    - chips/m6526.h
    - chips/m6569.h
    - chips/m6581.h
    - chips/kbd.h
    - chips/mem.h
    - chips/clk.h
    - systems/c1530.h
    - chips/m6522.h
    - systems/iecbus.h
    - systems/c1541.h

    ## The Commodore C64

    TODO!

    ## TODO:

    - floppy disc support

    ## Tests Status

    In chips-test/tests/testsuite-2.15/bin

        branchwrap:     ok
        cia1pb6:        ok
        cia1pb7:        ok
        cia1ta:         FAIL (OK, CIA sysclock not implemented)
        cia1tab:        ok
        cia1tb:         FAIL (OK, CIA sysclock not implemented)
        cia1tb123:      ok
        cia2pb6:        ok
        cia2pb7:        ok
        cia2ta:         FAIL (OK, CIA sysclock not implemented)
        cia2tb:         FAIL (OK, CIA sysclock not implemented)
        cntdef:         ok
        cnto2:          ok
        cpuport:        ok
        cputiming:      ok
        flipos:         ok
        icr01:          ok
        imr:            ok
        irq:            ok
        loadth:         ok
        mmu:            ok
        mmufetch:       ok
        nmi:            FAIL (1 error at 00/5)
        oneshot:        ok
        trap1..17:      ok

    In chips-test/tests/vice-tests/CIA:

    ciavarious:
        - all green, expect cia15.prg, which tests the CIA TOD clock,
          which isn't implemented

    cia-timer/cia-timer-oldcias.prg:
        - left side (CIA-1, IRQ) all green, right side (CIA-2, NMI) some red

    ciatimer/dd0dtest/dd0dtest.prg (NMI related):
        - some errors

    irqdelay:   all green

    mirrors/ciamirrors.prg: green

    reload0:
        reload0a.prg:   red
        reload0b.prg:   red

    shiftregister:
        cia-icr-test-continues-old.prg: green
        cia-icr-test-oneshot-old.prg: green
        cia-icr-test2-continues.prg: some red
        cia-icr-test2-oneshot.prg: some red
        cia-sp-test-continues-old.prg: much red (ok, CIA SP not implemented)
        cia-sp-test-oneshot-old.prg: much red (ok, CIA SP not implemented)

    timerbasics:    all green

    in chips-test/tests/vice-tests/interrupts:

    branchquirk:
        branchquirk-old.prg:    green
        branchquirk-nmiold.prg: red

    cia-int:
        cia-int-irq.prg:    green??
        cia-int-nmi.prg:    green??

    irq-ackn-bug:
        cia1.prg:       green
        cia2.prg:       green
        irq-ack-vicii.prg:  red
        irq-ackn_after_cli.prg: ???
        irq-ackn_after_cli2.prg: ???

    irqdma: (takes a long time)
        all fail?

    irqdummy/irqdummy.prg:  green

    irqnmi/irqnmi-old.prg: left (irq) green,right (nmi) red

    VICII:

    D011Test:                   TODO
    banking/banking.prg:        ok
    border:                     fail (border not opened)
    colorram/test.prg:          ok
    colorsplit/colorsplit.prg:  fail (horizontal offsets)
    dentest:    (these were mostly fixed by moving the raster interrupt check
                 in m6569.h to tick 63, which made the otherwise some tests
                 were flickering because a second raster interrupt wasn't stable)
        den01-48-0.prg:         ok
        den01-48-1.prg:         ok
        den01-48-2.prg:         ok
        den01-49-0.prg:         ok
        den01-49-1.prg:         ok
        den01-49-2.prg:         ok
        den10-48-0.prg:         ok
        den10-48-1.prg:         ok
        den10-48-2.prg:         FAIL
        den10-51-0.prg:         ok
        den10-51-1.prg:         ok
        den10-51-2.prg:         ok
        den10-51-3.prg:         ok
        denrsel-0.prg:          ok
        denrsel-1.prg:          ok
        denrsel-2.prg:          ok
        denrsel-63.prg:         ok
        denrsel-s0.prg:         ok
        denrsel-s1.prg:         ok
        denrsel-s2.prg:         ok
        denrsel55.prg:          ok
    dmadelay:
        test1-2a-03.prg:        ok
        test1-2a-04.prg:        FAIL (flickering)
        test1-2a-10.prg:        ok
        test1-2a-11.prg:        FAIL (1 char line off)
        test1-2a-16.prg:        ok
        test1-2a-17.prg:        FAIL (1 char line off)
        test1-2a-18.prg:        FAIL (1 char line/col off)
        test1.prg:              ??? (no reference image)
        test2-28-05.prg:        ok
        test2-28-06.prg:        FAIL (flickering)
        test2-28-11.prg:        ok
        test2-28-12.prg:        FAIL (one char line off)
        test2-28-16.prg:        ok
        test2-28-17.prg:        FAIL (one char line off)
        test2-28-18.prg:        FAIL (one char line/col off)
        test3-28-07.prg:        ok
        test3-28-08.prg:        FAIL (one char line off)
        test3-28-13.prg:        ok
        test3-28-14.prg:        FAIL (one char line off)
        test3-28-18.prg:        ok
        test3-28-19.prg:        FAIL (one char line off)
        test3-28-1a.prg:        FAIL (one char col off)

    fldscroll:  broken
    flibug/blackmail.prg:       reference image doesn't match

    frodotests:
        3fff.prg                ok
        d011h3.prg              FAIL (???)
        fld.prg                 ok
        lrborder:               FAIL (???)
        sprsync:                ok (???)
        stretch:                ok (???)
        tech-tech:              ok
        text26:                 ok

    gfxfetch/gfxfetch.prg:      FAIL (reference image doesn't match in boder)

    greydot/greydot.prg:        FAIL (ref image doesn't match, color bars start one tick late)

    lp-trigger:
        test1.prg:              FAIL (flickering)
        test2.prg:              FAIL

    lplatency/lplatency.prg:    FAIL

    movesplit:                  ????

    phi1timing:                 FAIL

    rasterirq:                  FAIL (reference image doesn't match)

    screenpos:                  FAIL (reference image doesn't match)

    split-tests:
        bascan          FAIL
        fetchsplit      FAIL (flickering characters)
        lightpen        FAIL
        modesplit       FAIL (ref image doesn't match)
        spritescan      FAIL

    sprite0move         ???

    spritebug           TODO
    all other sprite tests: TODO

    vicii-timing:       FAIL (ref image doesn't match)

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
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
#include <stdalign.h>
#include "iecbus.h"

#ifdef __cplusplus
extern "C" {
#endif

// bump snapshot version when c64_t memory layout changes
#define C64_SNAPSHOT_VERSION (1)

#define C64_FREQUENCY (985248)              // clock frequency in Hz
#define C64_MAX_AUDIO_SAMPLES (1024)        // max number of audio samples in internal sample buffer
#define C64_DEFAULT_AUDIO_SAMPLES (128)     // default number of samples in internal sample buffer

// C64 joystick types
typedef enum {
    C64_JOYSTICKTYPE_NONE,
    C64_JOYSTICKTYPE_DIGITAL_1,
    C64_JOYSTICKTYPE_DIGITAL_2,
    C64_JOYSTICKTYPE_DIGITAL_12,    // input routed to both joysticks
    C64_JOYSTICKTYPE_PADDLE_1,      // FIXME: not emulated
    C64_JOYSTICKTYPE_PADDLE_2,      // FIXME: not emulated
} c64_joystick_type_t;

// joystick mask bits
#define C64_JOYSTICK_UP    (1<<0)
#define C64_JOYSTICK_DOWN  (1<<1)
#define C64_JOYSTICK_LEFT  (1<<2)
#define C64_JOYSTICK_RIGHT (1<<3)
#define C64_JOYSTICK_BTN   (1<<4)

// CPU port memory mapping bits
#define C64_CPUPORT_LORAM (1<<0)
#define C64_CPUPORT_HIRAM (1<<1)
#define C64_CPUPORT_CHAREN (1<<2)

// casette port bits, same as C1530_CASPORT_*
#define C64_CASPORT_MOTOR   (1<<0)  // 1: motor off, 0: motor on
#define C64_CASPORT_READ    (1<<1)  // 1: read signal from datasette, connected to CIA-1 FLAG
#define C64_CASPORT_WRITE   (1<<2)  // not implemented
#define C64_CASPORT_SENSE   (1<<3)  // 1: play button up, 0: play button down

// // IEC port bits, same as C1541_IECPORT_*
// #define IECLINE_RESET   (1<<0)  // 1: RESET, 0: no reset
// #define IECLINE_SRQIN   (1<<1)  // connected to CIA-1 FLAG
// #define IECLINE_DATA    (1<<2)
// #define IECLINE_CLK     (1<<3)
// #define IECLINE_ATN     (1<<4)

// special keyboard keys
#define C64_KEY_SPACE    (0x20)     // space
#define C64_KEY_CSRLEFT  (0x08)     // cursor left (shift+cursor right)
#define C64_KEY_CSRRIGHT (0x09)     // cursor right
#define C64_KEY_CSRDOWN  (0x0A)     // cursor down
#define C64_KEY_CSRUP    (0x0B)     // cursor up (shift+cursor down)
#define C64_KEY_DEL      (0x01)     // delete
#define C64_KEY_INST     (0x10)     // inst (shift+del)
#define C64_KEY_HOME     (0x0C)     // home
#define C64_KEY_CLR      (0x02)     // clear (shift+home)
#define C64_KEY_RETURN   (0x0D)     // return
#define C64_KEY_CTRL     (0x0E)     // ctrl
#define C64_KEY_CBM      (0x0F)     // C= commodore key
#define C64_KEY_RESTORE  (0xFF)     // restore (connected to the NMI line)
#define C64_KEY_STOP     (0x03)     // run stop
#define C64_KEY_RUN      (0x07)     // run (shift+run stop)
#define C64_KEY_LEFT     (0x04)     // left arrow symbol
#define C64_KEY_F1       (0xF1)     // F1
#define C64_KEY_F2       (0xF2)     // F2
#define C64_KEY_F3       (0xF3)     // F3
#define C64_KEY_F4       (0xF4)     // F4
#define C64_KEY_F5       (0xF5)     // F5
#define C64_KEY_F6       (0xF6)     // F6
#define C64_KEY_F7       (0xF7)     // F7
#define C64_KEY_F8       (0xF8)     // F8

// config parameters for c64_init()
typedef struct {
    bool c1530_enabled;     // true to enable the C1530 datassette emulation
    bool c1541_enabled;     // true to enable the C1541 floppy drive emulation
    c64_joystick_type_t joystick_type;  // default is C64_JOYSTICK_NONE
    chips_debug_t debug;    // optional debugging hook
    chips_audio_desc_t audio;   // audio output options
    // ROM images
    struct {
        chips_range_t chars;     // 4 KByte character ROM dump
        chips_range_t basic;     // 8 KByte BASIC dump
        chips_range_t kernal;    // 8 KByte KERNAL dump
        // optional C1514 ROM images
        struct {
            chips_range_t c000_dfff;
            chips_range_t e000_ffff;
        } c1541;
    } roms;

#ifdef __IEC_DEBUG
    FILE* debug_file;
#endif
} c64_desc_t;

// C64 emulator state
typedef struct {
    m6502_t cpu;
    m6526_t cia_1;
    m6526_t cia_2;
    m6569_t vic;
    m6581_t sid;
    uint64_t pins;

    c64_joystick_type_t joystick_type;
    bool io_mapped;             // true when D000..DFFF has IO area mapped in
    uint8_t cas_port;           // cassette port, shared with c1530_t if datasette is connected
    iecbus_t iec_bus;           // iec bus to connect peripherals
    iecbus_device_t* iec_device; // the c64 device port on the IEC bus
    uint8_t cpu_port;           // last state of CPU port (for memory mapping)
    uint8_t kbd_joy1_mask;      // current joystick-1 state from keyboard-joystick emulation
    uint8_t kbd_joy2_mask;      // current joystick-2 state from keyboard-joystick emulation
    uint8_t joy_joy1_mask;      // current joystick-1 state from c64_joystick()
    uint8_t joy_joy2_mask;      // current joystick-2 state from c64_joystick()
    uint16_t vic_bank_select;   // upper 4 address bits from CIA-2 port A

    kbd_t kbd;                  // keyboard matrix state
    mem_t mem_cpu;              // CPU-visible memory mapping
    mem_t mem_vic;              // VIC-visible memory mapping
    bool valid;
    chips_debug_t debug;

    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        float sample_buffer[C64_MAX_AUDIO_SAMPLES];
    } audio;

    uint8_t color_ram[1024];        // special static color ram
    uint8_t ram[1<<16];             // general ram
    uint8_t rom_char[0x1000];       // 4 KB character ROM image
    uint8_t rom_basic[0x2000];      // 8 KB BASIC ROM image
    uint8_t rom_kernal[0x2000];     // 8 KB KERNAL V3 ROM image
    alignas(64) uint8_t fb[M6569_FRAMEBUFFER_SIZE_BYTES];

    c1530_t c1530;      // optional datassette
    c1541_t c1541;      // optional floppy drive

#ifdef __IEC_DEBUG
    FILE* debug_file;
#endif
} c64_t;

// initialize a new C64 instance
void c64_init(c64_t* sys, const c64_desc_t* desc);
// discard C64 instance
void c64_discard(c64_t* sys);
// reset a C64 instance
void c64_reset(c64_t* sys);
// get framebuffer and display attributes
chips_display_info_t c64_display_info(c64_t* sys);
// tick C64 instance for a given number of microseconds, return number of ticks executed
uint32_t c64_exec(c64_t* sys, uint32_t micro_seconds);
// send a key-down event to the C64
void c64_key_down(c64_t* sys, int key_code);
// send a key-up event to the C64
void c64_key_up(c64_t* sys, int key_code);
// enable/disable joystick emulation
void c64_set_joystick_type(c64_t* sys, c64_joystick_type_t type);
// get current joystick emulation type
c64_joystick_type_t c64_joystick_type(c64_t* sys);
// set joystick mask (combination of C64_JOYSTICK_*)
void c64_joystick(c64_t* sys, uint8_t joy1_mask, uint8_t joy2_mask);
// quickload a .bin/.prg file
bool c64_quickload(c64_t* sys, chips_range_t data);
// insert tape as .TAP file (c1530 must be enabled)
bool c64_insert_tape(c64_t* sys, chips_range_t data);
// remove tape file
void c64_remove_tape(c64_t* sys);
// return true if a tape is currently inserted
bool c64_tape_inserted(c64_t* sys);
// start the tape (press the Play button)
void c64_tape_play(c64_t* sys);
// stop the tape (unpress the Play button
void c64_tape_stop(c64_t* sys);
// return true if tape motor is on
bool c64_is_tape_motor_on(c64_t* sys);
// save a snapshot, patches pointers to zero and offsets, returns snapshot version
uint32_t c64_save_snapshot(c64_t* sys, c64_t* dst);
// load a snapshot, returns false if snapshot versions don't match
bool c64_load_snapshot(c64_t* sys, uint32_t version, c64_t* src);
// perform a RUN BASIC call
void c64_basic_run(c64_t* sys);
// perform a LOAD BASIC call
void c64_basic_load(c64_t* sys);
// perform a SYS xxxx BASIC call via the BASIC input buffer
void c64_basic_syscall(c64_t* sys, uint16_t addr);
// returns the SYS call return address (can be used to set a breakpoint)
uint16_t c64_syscall_return_addr(void);

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h> // memcpy, memset
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _C64_SCREEN_WIDTH (392)
#define _C64_SCREEN_HEIGHT (272)
#define _C64_SCREEN_X (64)
#define _C64_SCREEN_Y (24)

static uint8_t _c64_cpu_port_in(void* user_data);
static void _c64_cpu_port_out(uint8_t data, void* user_data);
static uint16_t _c64_vic_fetch(uint16_t addr, void* user_data);
static void _c64_update_memory_map(c64_t* sys);
static void _c64_init_key_map(c64_t* sys);
static void _c64_init_memory_map(c64_t* sys);

#define _C64_DEFAULT(val,def) (((val) != 0) ? (val) : (def))

void c64_init(c64_t* sys, const c64_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) { CHIPS_ASSERT(desc->debug.stopped); }

    memset(sys, 0, sizeof(c64_t));
#ifdef __IEC_DEBUG
    sys->debug_file = desc->debug_file;
#endif
    sys->valid = true;
    sys->joystick_type = desc->joystick_type;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _C64_DEFAULT(desc->audio.num_samples, C64_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= C64_MAX_AUDIO_SAMPLES);
    CHIPS_ASSERT(desc->roms.chars.ptr && (desc->roms.chars.size == sizeof(sys->rom_char)));
    CHIPS_ASSERT(desc->roms.basic.ptr && (desc->roms.basic.size == sizeof(sys->rom_basic)));
    CHIPS_ASSERT(desc->roms.kernal.ptr && (desc->roms.kernal.size == sizeof(sys->rom_kernal)));
    memcpy(sys->rom_char, desc->roms.chars.ptr, sizeof(sys->rom_char));
    memcpy(sys->rom_basic, desc->roms.basic.ptr, sizeof(sys->rom_basic));
    memcpy(sys->rom_kernal, desc->roms.kernal.ptr, sizeof(sys->rom_kernal));

    // initialize the hardware
    sys->cpu_port = 0xF7;       // for initial memory mapping
    sys->io_mapped = true;
    sys->cas_port = C64_CASPORT_MOTOR|C64_CASPORT_SENSE;

    sys->pins = m6502_init(&sys->cpu, &(m6502_desc_t) {
        .m6510_in_cb = _c64_cpu_port_in,
        .m6510_out_cb = _c64_cpu_port_out,
        .m6510_io_pullup = 0x17,
        .m6510_io_floating = 0xC8,
        .m6510_user_data = sys,
    });
    m6526_init(&sys->cia_1);
    m6526_init(&sys->cia_2);
    m6569_init(&sys->vic, &(m6569_desc_t){
        .fetch_cb = _c64_vic_fetch,
        .framebuffer = {
            .ptr = sys->fb,
            .size = sizeof(sys->fb),
        },
        .screen = {
            .x = _C64_SCREEN_X,
            .y = _C64_SCREEN_Y,
            .width = _C64_SCREEN_WIDTH,
            .height = _C64_SCREEN_HEIGHT,
        },
        .user_data = sys,
    });
    m6581_init(&sys->sid, &(m6581_desc_t){
        .tick_hz = C64_FREQUENCY,
        .sound_hz = _C64_DEFAULT(desc->audio.sample_rate, 44100),
        .magnitude = _C64_DEFAULT(desc->audio.volume, 1.0f),
    });
    _c64_init_key_map(sys);
    _c64_init_memory_map(sys);
    // Connect to IEC bus and tell it that we have inverter logic in our hardware
    sys->iec_device = iec_connect(&sys->iec_bus, true);
    CHIPS_ASSERT(sys->iec_device);
    if (desc->c1530_enabled) {
        c1530_init(&sys->c1530, &(c1530_desc_t){
            .cas_port = &sys->cas_port,
        });
    }
    if (desc->c1541_enabled) {
        c1541_init(&sys->c1541, &(c1541_desc_t){
            .iec_bus = &sys->iec_bus,
            .roms = {
                .c000_dfff = desc->roms.c1541.c000_dfff,
                .e000_ffff = desc->roms.c1541.e000_ffff
            },
#ifdef __IEC_DEBUG
            .debug_file = desc->debug_file,
#endif
        });
    }
}

void c64_discard(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
    if (sys->c1530.valid) {
        c1530_discard(&sys->c1530);
    }
    if (sys->c1541.valid) {
        c1541_discard(&sys->c1541);
    }
}

void c64_reset(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->cpu_port = 0xF7;
    sys->kbd_joy1_mask = sys->kbd_joy2_mask = 0;
    sys->joy_joy1_mask = sys->joy_joy2_mask = 0;
    sys->io_mapped = true;
    sys->cas_port = C64_CASPORT_MOTOR|C64_CASPORT_SENSE;
    _c64_update_memory_map(sys);
    sys->pins |= M6502_RES;
    m6526_reset(&sys->cia_1);
    m6526_reset(&sys->cia_2);
    m6569_reset(&sys->vic);
    m6581_reset(&sys->sid);
}

#ifdef __IEC_DEBUG
uint16_t _get_c64_nearest_routine_address(uint16_t addr) {
    uint16_t res = 0;
    if (addr >= 0xFFF3) res = 0xFFF3;
    else if (addr >= 0xFFF0) res = 0xFFF0;
    else if (addr >= 0xFFED) res = 0xFFED;
    else if (addr >= 0xFFE7) res = 0xFFE7;
    else if (addr >= 0xFFE4) res = 0xFFE4;
    else if (addr >= 0xFFE1) res = 0xFFE1;
    else if (addr >= 0xFFD2) res = 0xFFD2;
    else if (addr >= 0xFFCF) res = 0xFFCF;
    else if (addr >= 0xFFCC) res = 0xFFCC;
    else if (addr >= 0xFFC9) res = 0xFFC9;
    else if (addr >= 0xFFC6) res = 0xFFC6;
    else if (addr >= 0xFFC3) res = 0xFFC3;
    else if (addr >= 0xFFC0) res = 0xFFC0;
    else if (addr >= 0xFF58) res = 0xFF58;
    else if (addr >= 0xFF48) res = 0xFF48;
    else if (addr >= 0xFF43) res = 0xFF43;
    else if (addr >= 0xFF2E) res = 0xFF2E;
    else if (addr >= 0xFED6) res = 0xFED6;
    else if (addr >= 0xFEBC) res = 0xFEBC;
    else if (addr >= 0xFEB6) res = 0xFEB6;
    else if (addr >= 0xFEAE) res = 0xFEAE;
    else if (addr >= 0xFEA3) res = 0xFEA3;
    else if (addr >= 0xFE9D) res = 0xFE9D;
    else if (addr >= 0xFE9A) res = 0xFE9A;
    else if (addr >= 0xFE72) res = 0xFE72;
    else if (addr >= 0xFE66) res = 0xFE66;
    else if (addr >= 0xFE56) res = 0xFE56;
    else if (addr >= 0xFE4C) res = 0xFE4C;
    else if (addr >= 0xFE47) res = 0xFE47;
    else if (addr >= 0xFE43) res = 0xFE43;
    else if (addr >= 0xFE3C) res = 0xFE3C;
    else if (addr >= 0xFE34) res = 0xFE34;
    else if (addr >= 0xFE2D) res = 0xFE2D;
    else if (addr >= 0xFE27) res = 0xFE27;
    else if (addr >= 0xFE25) res = 0xFE25;
    else if (addr >= 0xFE21) res = 0xFE21;
    else if (addr >= 0xFE1C) res = 0xFE1C;
    else if (addr >= 0xFE1A) res = 0xFE1A;
    else if (addr >= 0xFE18) res = 0xFE18;
    else if (addr >= 0xFE07) res = 0xFE07;
    else if (addr >= 0xFE00) res = 0xFE00;
    else if (addr >= 0xFDF9) res = 0xFDF9;
    else if (addr >= 0xFDF3) res = 0xFDF3;
    else if (addr >= 0xFDEC) res = 0xFDEC;
    else if (addr >= 0xFDDD) res = 0xFDDD;
    else if (addr >= 0xFDA3) res = 0xFDA3;
    else if (addr >= 0xFD9B) res = 0xFD9B;
    else if (addr >= 0xFD88) res = 0xFD88;
    else if (addr >= 0xFD6E) res = 0xFD6E;
    else if (addr >= 0xFD6C) res = 0xFD6C;
    else if (addr >= 0xFD53) res = 0xFD53;
    else if (addr >= 0xFD50) res = 0xFD50;
    else if (addr >= 0xFD27) res = 0xFD27;
    else if (addr >= 0xFD20) res = 0xFD20;
    else if (addr >= 0xFD1A) res = 0xFD1A;
    else if (addr >= 0xFD15) res = 0xFD15;
    else if (addr >= 0xFD10) res = 0xFD10;
    else if (addr >= 0xFD0F) res = 0xFD0F;
    else if (addr >= 0xFD04) res = 0xFD04;
    else if (addr >= 0xFD02) res = 0xFD02;
    else if (addr >= 0xFCEF) res = 0xFCEF;
    else if (addr >= 0xFCE2) res = 0xFCE2;
    else if (addr >= 0xFCE1) res = 0xFCE1;
    else if (addr >= 0xFCDB) res = 0xFCDB;
    else if (addr >= 0xFCD1) res = 0xFCD1;
    else if (addr >= 0xFCCA) res = 0xFCCA;
    else if (addr >= 0xFCBD) res = 0xFCBD;
    else if (addr >= 0xFCB8) res = 0xFCB8;
    else if (addr >= 0xFCB6) res = 0xFCB6;
    else if (addr >= 0xFC93) res = 0xFC93;
    else if (addr >= 0xFC6A) res = 0xFC6A;
    else if (addr >= 0xFC5E) res = 0xFC5E;
    else if (addr >= 0xFC57) res = 0xFC57;
    else if (addr >= 0xFC54) res = 0xFC54;
    else if (addr >= 0xFC4E) res = 0xFC4E;
    else if (addr >= 0xFC3F) res = 0xFC3F;
    else if (addr >= 0xFC30) res = 0xFC30;
    else if (addr >= 0xFC2C) res = 0xFC2C;
    else if (addr >= 0xFC22) res = 0xFC22;
    else if (addr >= 0xFC16) res = 0xFC16;
    else if (addr >= 0xFC0C) res = 0xFC0C;
    else if (addr >= 0xFC09) res = 0xFC09;
    else if (addr >= 0xFBF0) res = 0xFBF0;
    else if (addr >= 0xFBE3) res = 0xFBE3;
    else if (addr >= 0xFBCD) res = 0xFBCD;
    else if (addr >= 0xFBC8) res = 0xFBC8;
    else if (addr >= 0xFBB1) res = 0xFBB1;
    else if (addr >= 0xFBAF) res = 0xFBAF;
    else if (addr >= 0xFBAD) res = 0xFBAD;
    else if (addr >= 0xFBA6) res = 0xFBA6;
    else if (addr >= 0xFB97) res = 0xFB97;
    else if (addr >= 0xFB8E) res = 0xFB8E;
    else if (addr >= 0xFB8B) res = 0xFB8B;
    else if (addr >= 0xFB72) res = 0xFB72;
    else if (addr >= 0xFB68) res = 0xFB68;
    else if (addr >= 0xFB5C) res = 0xFB5C;
    else if (addr >= 0xFB4A) res = 0xFB4A;
    else if (addr >= 0xFB48) res = 0xFB48;
    else if (addr >= 0xFB43) res = 0xFB43;
    else if (addr >= 0xFB3A) res = 0xFB3A;
    else if (addr >= 0xFB33) res = 0xFB33;
    else if (addr >= 0xFB2F) res = 0xFB2F;
    else if (addr >= 0xFB08) res = 0xFB08;
    else if (addr >= 0xFAEB) res = 0xFAEB;
    else if (addr >= 0xFACE) res = 0xFACE;
    else if (addr >= 0xFAC0) res = 0xFAC0;
    else if (addr >= 0xFABA) res = 0xFABA;
    else if (addr >= 0xFAA9) res = 0xFAA9;
    else if (addr >= 0xFAA3) res = 0xFAA3;
    else if (addr >= 0xFA8D) res = 0xFA8D;
    else if (addr >= 0xFA8A) res = 0xFA8A;
    else if (addr >= 0xFA86) res = 0xFA86;
    else if (addr >= 0xFA70) res = 0xFA70;
    else if (addr >= 0xFA60) res = 0xFA60;
    else if (addr >= 0xFA5D) res = 0xFA5D;
    else if (addr >= 0xFA53) res = 0xFA53;
    else if (addr >= 0xFA44) res = 0xFA44;
    else if (addr >= 0xFA1F) res = 0xFA1F;
    else if (addr >= 0xFA18) res = 0xFA18;
    else if (addr >= 0xFA10) res = 0xFA10;
    else if (addr >= 0xF9F7) res = 0xF9F7;
    else if (addr >= 0xF9E0) res = 0xF9E0;
    else if (addr >= 0xF9DE) res = 0xF9DE;
    else if (addr >= 0xF9D5) res = 0xF9D5;
    else if (addr >= 0xF9D2) res = 0xF9D2;
    else if (addr >= 0xF9C9) res = 0xF9C9;
    else if (addr >= 0xF9BC) res = 0xF9BC;
    else if (addr >= 0xF9AC) res = 0xF9AC;
    else if (addr >= 0xF999) res = 0xF999;
    else if (addr >= 0xF997) res = 0xF997;
    else if (addr >= 0xF993) res = 0xF993;
    else if (addr >= 0xF98B) res = 0xF98B;
    else if (addr >= 0xF988) res = 0xF988;
    else if (addr >= 0xF969) res = 0xF969;
    else if (addr >= 0xF92C) res = 0xF92C;
    else if (addr >= 0xF92A) res = 0xF92A;
    else if (addr >= 0xF8FE) res = 0xF8FE;
    else if (addr >= 0xF8F7) res = 0xF8F7;
    else if (addr >= 0xF8E2) res = 0xF8E2;
    else if (addr >= 0xF8E1) res = 0xF8E1;
    else if (addr >= 0xF8DC) res = 0xF8DC;
    else if (addr >= 0xF8D0) res = 0xF8D0;
    else if (addr >= 0xF8BE) res = 0xF8BE;
    else if (addr >= 0xF8B7) res = 0xF8B7;
    else if (addr >= 0xF8B5) res = 0xF8B5;
    else if (addr >= 0xF875) res = 0xF875;
    else if (addr >= 0xF86E) res = 0xF86E;
    else if (addr >= 0xF86B) res = 0xF86B;
    else if (addr >= 0xF867) res = 0xF867;
    else if (addr >= 0xF864) res = 0xF864;
    else if (addr >= 0xF84A) res = 0xF84A;
    else if (addr >= 0xF841) res = 0xF841;
    else if (addr >= 0xF838) res = 0xF838;
    else if (addr >= 0xF836) res = 0xF836;
    else if (addr >= 0xF82E) res = 0xF82E;
    else if (addr >= 0xF821) res = 0xF821;
    else if (addr >= 0xF81E) res = 0xF81E;
    else if (addr >= 0xF817) res = 0xF817;
    else if (addr >= 0xF80D) res = 0xF80D;
    else if (addr >= 0xF80C) res = 0xF80C;
    else if (addr >= 0xF80B) res = 0xF80B;
    else if (addr >= 0xF7F7) res = 0xF7F7;
    else if (addr >= 0xF7EA) res = 0xF7EA;
    else if (addr >= 0xF7D7) res = 0xF7D7;
    else if (addr >= 0xF7D0) res = 0xF7D0;
    else if (addr >= 0xF7CF) res = 0xF7CF;
    else if (addr >= 0xF7B7) res = 0xF7B7;
    else if (addr >= 0xF7A5) res = 0xF7A5;
    else if (addr >= 0xF781) res = 0xF781;
    else if (addr >= 0xF76A) res = 0xF76A;
    else if (addr >= 0xF769) res = 0xF769;
    else if (addr >= 0xF767) res = 0xF767;
    else if (addr >= 0xF761) res = 0xF761;
    else if (addr >= 0xF757) res = 0xF757;
    else if (addr >= 0xF74B) res = 0xF74B;
    else if (addr >= 0xF72C) res = 0xF72C;
    else if (addr >= 0xF729) res = 0xF729;
    else if (addr >= 0xF713) res = 0xF713;
    else if (addr >= 0xF710) res = 0xF710;
    else if (addr >= 0xF70D) res = 0xF70D;
    else if (addr >= 0xF70A) res = 0xF70A;
    else if (addr >= 0xF707) res = 0xF707;
    else if (addr >= 0xF704) res = 0xF704;
    else if (addr >= 0xF701) res = 0xF701;
    else if (addr >= 0xF6FE) res = 0xF6FE;
    else if (addr >= 0xF6FB) res = 0xF6FB;
    else if (addr >= 0xF6FA) res = 0xF6FA;
    else if (addr >= 0xF6ED) res = 0xF6ED;
    else if (addr >= 0xF6E4) res = 0xF6E4;
    else if (addr >= 0xF6DD) res = 0xF6DD;
    else if (addr >= 0xF6DC) res = 0xF6DC;
    else if (addr >= 0xF6DA) res = 0xF6DA;
    else if (addr >= 0xF6CC) res = 0xF6CC;
    else if (addr >= 0xF6BC) res = 0xF6BC;
    else if (addr >= 0xF6A7) res = 0xF6A7;
    else if (addr >= 0xF69D) res = 0xF69D;
    else if (addr >= 0xF69B) res = 0xF69B;
    else if (addr >= 0xF68F) res = 0xF68F;
    else if (addr >= 0xF68E) res = 0xF68E;
    else if (addr >= 0xF68D) res = 0xF68D;
    else if (addr >= 0xF676) res = 0xF676;
    else if (addr >= 0xF66C) res = 0xF66C;
    else if (addr >= 0xF65F) res = 0xF65F;
    else if (addr >= 0xF659) res = 0xF659;
    else if (addr >= 0xF657) res = 0xF657;
    else if (addr >= 0xF654) res = 0xF654;
    else if (addr >= 0xF642) res = 0xF642;
    else if (addr >= 0xF63F) res = 0xF63F;
    else if (addr >= 0xF63A) res = 0xF63A;
    else if (addr >= 0xF633) res = 0xF633;
    else if (addr >= 0xF624) res = 0xF624;
    else if (addr >= 0xF605) res = 0xF605;
    else if (addr >= 0xF5F4) res = 0xF5F4;
    else if (addr >= 0xF5F1) res = 0xF5F1;
    else if (addr >= 0xF5ED) res = 0xF5ED;
    else if (addr >= 0xF5EA) res = 0xF5EA;
    else if (addr >= 0xF5DD) res = 0xF5DD;
    else if (addr >= 0xF5DA) res = 0xF5DA;
    else if (addr >= 0xF5D2) res = 0xF5D2;
    else if (addr >= 0xF5D1) res = 0xF5D1;
    else if (addr >= 0xF5C7) res = 0xF5C7;
    else if (addr >= 0xF5C1) res = 0xF5C1;
    else if (addr >= 0xF5AF) res = 0xF5AF;
    else if (addr >= 0xF5AE) res = 0xF5AE;
    else if (addr >= 0xF5A9) res = 0xF5A9;
    else if (addr >= 0xF57D) res = 0xF57D;
    else if (addr >= 0xF579) res = 0xF579;
    else if (addr >= 0xF56C) res = 0xF56C;
    else if (addr >= 0xF55D) res = 0xF55D;
    else if (addr >= 0xF556) res = 0xF556;
    else if (addr >= 0xF549) res = 0xF549;
    else if (addr >= 0xF541) res = 0xF541;
    else if (addr >= 0xF539) res = 0xF539;
    else if (addr >= 0xF533) res = 0xF533;
    else if (addr >= 0xF530) res = 0xF530;
    else if (addr >= 0xF524) res = 0xF524;
    else if (addr >= 0xF51E) res = 0xF51E;
    else if (addr >= 0xF51C) res = 0xF51C;
    else if (addr >= 0xF501) res = 0xF501;
    else if (addr >= 0xF4F3) res = 0xF4F3;
    else if (addr >= 0xF4F0) res = 0xF4F0;
    else if (addr >= 0xF4BF) res = 0xF4BF;
    else if (addr >= 0xF4B2) res = 0xF4B2;
    else if (addr >= 0xF4AF) res = 0xF4AF;
    else if (addr >= 0xF4A5) res = 0xF4A5;
    else if (addr >= 0xF4A2) res = 0xF4A2;
    else if (addr >= 0xF49E) res = 0xF49E;
    else if (addr >= 0xF483) res = 0xF483;
    else if (addr >= 0xF47D) res = 0xF47D;
    else if (addr >= 0xF474) res = 0xF474;
    else if (addr >= 0xF468) res = 0xF468;
    else if (addr >= 0xF45C) res = 0xF45C;
    else if (addr >= 0xF44D) res = 0xF44D;
    else if (addr >= 0xF446) res = 0xF446;
    else if (addr >= 0xF440) res = 0xF440;
    else if (addr >= 0xF43A) res = 0xF43A;
    else if (addr >= 0xF41D) res = 0xF41D;
    else if (addr >= 0xF40F) res = 0xF40F;
    else if (addr >= 0xF409) res = 0xF409;
    else if (addr >= 0xF406) res = 0xF406;
    else if (addr >= 0xF3FC) res = 0xF3FC;
    else if (addr >= 0xF3F6) res = 0xF3F6;
    else if (addr >= 0xF3D5) res = 0xF3D5;
    else if (addr >= 0xF3D4) res = 0xF3D4;
    else if (addr >= 0xF3D3) res = 0xF3D3;
    else if (addr >= 0xF3D1) res = 0xF3D1;
    else if (addr >= 0xF3C2) res = 0xF3C2;
    else if (addr >= 0xF3B8) res = 0xF3B8;
    else if (addr >= 0xF3AF) res = 0xF3AF;
    else if (addr >= 0xF3AC) res = 0xF3AC;
    else if (addr >= 0xF393) res = 0xF393;
    else if (addr >= 0xF38B) res = 0xF38B;
    else if (addr >= 0xF384) res = 0xF384;
    else if (addr >= 0xF362) res = 0xF362;
    else if (addr >= 0xF359) res = 0xF359;
    else if (addr >= 0xF351) res = 0xF351;
    else if (addr >= 0xF34A) res = 0xF34A;
    else if (addr >= 0xF343) res = 0xF343;
    else if (addr >= 0xF33C) res = 0xF33C;
    else if (addr >= 0xF333) res = 0xF333;
    else if (addr >= 0xF32F) res = 0xF32F;
    else if (addr >= 0xF32E) res = 0xF32E;
    else if (addr >= 0xF31F) res = 0xF31F;
    else if (addr >= 0xF316) res = 0xF316;
    else if (addr >= 0xF314) res = 0xF314;
    else if (addr >= 0xF30F) res = 0xF30F;
    else if (addr >= 0xF30E) res = 0xF30E;
    else if (addr >= 0xF30D) res = 0xF30D;
    else if (addr >= 0xF2F2) res = 0xF2F2;
    else if (addr >= 0xF2F1) res = 0xF2F1;
    else if (addr >= 0xF2EE) res = 0xF2EE;
    else if (addr >= 0xF2E0) res = 0xF2E0;
    else if (addr >= 0xF2C8) res = 0xF2C8;
    else if (addr >= 0xF2BF) res = 0xF2BF;
    else if (addr >= 0xF2BA) res = 0xF2BA;
    else if (addr >= 0xF298) res = 0xF298;
    else if (addr >= 0xF291) res = 0xF291;
    else if (addr >= 0xF289) res = 0xF289;
    else if (addr >= 0xF286) res = 0xF286;
    else if (addr >= 0xF279) res = 0xF279;
    else if (addr >= 0xF275) res = 0xF275;
    else if (addr >= 0xF26F) res = 0xF26F;
    else if (addr >= 0xF262) res = 0xF262;
    else if (addr >= 0xF25F) res = 0xF25F;
    else if (addr >= 0xF258) res = 0xF258;
    else if (addr >= 0xF250) res = 0xF250;
    else if (addr >= 0xF248) res = 0xF248;
    else if (addr >= 0xF245) res = 0xF245;
    else if (addr >= 0xF237) res = 0xF237;
    else if (addr >= 0xF233) res = 0xF233;
    else if (addr >= 0xF22A) res = 0xF22A;
    else if (addr >= 0xF216) res = 0xF216;
    else if (addr >= 0xF20E) res = 0xF20E;
    else if (addr >= 0xF208) res = 0xF208;
    else if (addr >= 0xF207) res = 0xF207;
    else if (addr >= 0xF1FD) res = 0xF1FD;
    else if (addr >= 0xF1FC) res = 0xF1FC;
    else if (addr >= 0xF1F8) res = 0xF1F8;
    else if (addr >= 0xF1DD) res = 0xF1DD;
    else if (addr >= 0xF1DB) res = 0xF1DB;
    else if (addr >= 0xF1CA) res = 0xF1CA;
    else if (addr >= 0xF1B8) res = 0xF1B8;
    else if (addr >= 0xF1B5) res = 0xF1B5;
    else if (addr >= 0xF1B4) res = 0xF1B4;
    else if (addr >= 0xF1B3) res = 0xF1B3;
    else if (addr >= 0xF1B1) res = 0xF1B1;
    else if (addr >= 0xF1AD) res = 0xF1AD;
    else if (addr >= 0xF1A9) res = 0xF1A9;
    else if (addr >= 0xF199) res = 0xF199;
    else if (addr >= 0xF196) res = 0xF196;
    else if (addr >= 0xF193) res = 0xF193;
    else if (addr >= 0xF18D) res = 0xF18D;
    else if (addr >= 0xF173) res = 0xF173;
    else if (addr >= 0xF166) res = 0xF166;
    else if (addr >= 0xF157) res = 0xF157;
    else if (addr >= 0xF155) res = 0xF155;
    else if (addr >= 0xF14E) res = 0xF14E;
    else if (addr >= 0xF14A) res = 0xF14A;
    else if (addr >= 0xF13E) res = 0xF13E;
    else if (addr >= 0xF13C) res = 0xF13C;
    else if (addr >= 0xF12F) res = 0xF12F;
    else if (addr >= 0xF12B) res = 0xF12B;
    else if (addr >= 0xF0BB) res = 0xF0BB;
    else if (addr >= 0xF0AA) res = 0xF0AA;
    else if (addr >= 0xF0A4) res = 0xF0A4;
    else if (addr >= 0xF09C) res = 0xF09C;
    else if (addr >= 0xF086) res = 0xF086;
    else if (addr >= 0xF084) res = 0xF084;
    else if (addr >= 0xF07D) res = 0xF07D;
    else if (addr >= 0xF077) res = 0xF077;
    else if (addr >= 0xF070) res = 0xF070;
    else if (addr >= 0xF062) res = 0xF062;
    else if (addr >= 0xF04D) res = 0xF04D;
    else if (addr >= 0xF04C) res = 0xF04C;
    else if (addr >= 0xF02E) res = 0xF02E;
    else if (addr >= 0xF028) res = 0xF028;
    else if (addr >= 0xF017) res = 0xF017;
    else if (addr >= 0xF014) res = 0xF014;
    else if (addr >= 0xF012) res = 0xF012;
    else if (addr >= 0xF00D) res = 0xF00D;
    else if (addr >= 0xF006) res = 0xF006;
    else if (addr >= 0xEFF9) res = 0xEFF9;
    else if (addr >= 0xEFF2) res = 0xEFF2;
    else if (addr >= 0xEFE1) res = 0xEFE1;
    else if (addr >= 0xEFDB) res = 0xEFDB;
    else if (addr >= 0xEFD2) res = 0xEFD2;
    else if (addr >= 0xEFD0) res = 0xEFD0;
    else if (addr >= 0xEFCD) res = 0xEFCD;
    else if (addr >= 0xEFCA) res = 0xEFCA;
    else if (addr >= 0xEFC5) res = 0xEFC5;
    else if (addr >= 0xEFB1) res = 0xEFB1;
    else if (addr >= 0xEFA9) res = 0xEFA9;
    else if (addr >= 0xEF97) res = 0xEF97;
    else if (addr >= 0xEF90) res = 0xEF90;
    else if (addr >= 0xEF8B) res = 0xEF8B;
    else if (addr >= 0xEF7E) res = 0xEF7E;
    else if (addr >= 0xEF70) res = 0xEF70;
    else if (addr >= 0xEF6E) res = 0xEF6E;
    else if (addr >= 0xEF6D) res = 0xEF6D;
    else if (addr >= 0xEF59) res = 0xEF59;
    else if (addr >= 0xEF58) res = 0xEF58;
    else if (addr >= 0xEF54) res = 0xEF54;
    else if (addr >= 0xEF4A) res = 0xEF4A;
    else if (addr >= 0xEF3B) res = 0xEF3B;
    else if (addr >= 0xEF39) res = 0xEF39;
    else if (addr >= 0xEF31) res = 0xEF31;
    else if (addr >= 0xEF2E) res = 0xEF2E;
    else if (addr >= 0xEF1E) res = 0xEF1E;
    else if (addr >= 0xEF1C) res = 0xEF1C;
    else if (addr >= 0xEF13) res = 0xEF13;
    else if (addr >= 0xEF06) res = 0xEF06;
    else if (addr >= 0xEF00) res = 0xEF00;
    else if (addr >= 0xEEFC) res = 0xEEFC;
    else if (addr >= 0xEEF6) res = 0xEEF6;
    else if (addr >= 0xEEE7) res = 0xEEE7;
    else if (addr >= 0xEEE6) res = 0xEEE6;
    else if (addr >= 0xEED7) res = 0xEED7;
    else if (addr >= 0xEED1) res = 0xEED1;
    else if (addr >= 0xEEC8) res = 0xEEC8;
    else if (addr >= 0xEEBB) res = 0xEEBB;
    else if (addr >= 0xEEB6) res = 0xEEB6;
    else if (addr >= 0xEEA9) res = 0xEEA9;
    else if (addr >= 0xEE80) res = 0xEE80;
    else if (addr >= 0xEE67) res = 0xEE67;
    else if (addr >= 0xEE5A) res = 0xEE5A;
    else if (addr >= 0xEE56) res = 0xEE56;
    else if (addr >= 0xEE47) res = 0xEE47;
    else if (addr >= 0xEE3E) res = 0xEE3E;
    else if (addr >= 0xEE30) res = 0xEE30;
    else if (addr >= 0xEE1B) res = 0xEE1B;
    else if (addr >= 0xEE09) res = 0xEE09;
    else if (addr >= 0xEE06) res = 0xEE06;
    else if (addr >= 0xEE03) res = 0xEE03;
    else if (addr >= 0xEDFE) res = 0xEDFE;
    else if (addr >= 0xEDEF) res = 0xEDEF;
    else if (addr >= 0xEDEB) res = 0xEDEB;
    else if (addr >= 0xEDE6) res = 0xEDE6;
    else if (addr >= 0xEDDD) res = 0xEDDD;
    else if (addr >= 0xEDD6) res = 0xEDD6;
    else if (addr >= 0xEDC7) res = 0xEDC7;
    else if (addr >= 0xEDBE) res = 0xEDBE;
    else if (addr >= 0xEDB9) res = 0xEDB9;
    else if (addr >= 0xEDB2) res = 0xEDB2;
    else if (addr >= 0xEDAF) res = 0xEDAF;
    else if (addr >= 0xEDAD) res = 0xEDAD;
    else if (addr >= 0xED9F) res = 0xED9F;
    else if (addr >= 0xED7D) res = 0xED7D;
    else if (addr >= 0xED7A) res = 0xED7A;
    else if (addr >= 0xED66) res = 0xED66;
    else if (addr >= 0xED5A) res = 0xED5A;
    else if (addr >= 0xED55) res = 0xED55;
    else if (addr >= 0xED50) res = 0xED50;
    else if (addr >= 0xED40) res = 0xED40;
    else if (addr >= 0xED36) res = 0xED36;
    else if (addr >= 0xED2E) res = 0xED2E;
    else if (addr >= 0xED20) res = 0xED20;
    else if (addr >= 0xED11) res = 0xED11;
    else if (addr >= 0xED0C) res = 0xED0C;
    else if (addr >= 0xED09) res = 0xED09;
    else if (addr >= 0xECE7) res = 0xECE7;
    else if (addr >= 0xEC72) res = 0xEC72;
    else if (addr >= 0xEC5B) res = 0xEC5B;
    else if (addr >= 0xEC58) res = 0xEC58;
    else if (addr >= 0xEB59) res = 0xEB59;
    else if (addr >= 0xEB42) res = 0xEB42;
    else if (addr >= 0xEB30) res = 0xEB30;
    else if (addr >= 0xEB17) res = 0xEB17;
    else if (addr >= 0xEB0D) res = 0xEB0D;
    else if (addr >= 0xEAFB) res = 0xEAFB;
    else if (addr >= 0xEAF0) res = 0xEAF0;
    else if (addr >= 0xEAE0) res = 0xEAE0;
    else if (addr >= 0xEADC) res = 0xEADC;
    else if (addr >= 0xEACC) res = 0xEACC;
    else if (addr >= 0xEACB) res = 0xEACB;
    else if (addr >= 0xEAB3) res = 0xEAB3;
    else if (addr >= 0xEAAB) res = 0xEAAB;
    else if (addr >= 0xEAA8) res = 0xEAA8;
    else if (addr >= 0xEA87) res = 0xEA87;
    else if (addr >= 0xEA7E) res = 0xEA7E;
    else if (addr >= 0xEA7B) res = 0xEA7B;
    else if (addr >= 0xEA71) res = 0xEA71;
    else if (addr >= 0xEA61) res = 0xEA61;
    else if (addr >= 0xEA5C) res = 0xEA5C;
    else if (addr >= 0xEA3E) res = 0xEA3E;
    else if (addr >= 0xEA31) res = 0xEA31;
    else if (addr >= 0xEA24) res = 0xEA24;
    else if (addr >= 0xEA1C) res = 0xEA1C;
    else if (addr >= 0xEA13) res = 0xEA13;
    else if (addr >= 0xEA07) res = 0xEA07;
    else if (addr >= 0xE9FF) res = 0xE9FF;
    else if (addr >= 0xE9F0) res = 0xE9F0;
    else if (addr >= 0xE9BA) res = 0xE9BA;
    else if (addr >= 0xE98F) res = 0xE98F;
    else if (addr >= 0xE981) res = 0xE981;
    else if (addr >= 0xE96C) res = 0xE96C;
    else if (addr >= 0xE967) res = 0xE967;
    else if (addr >= 0xE958) res = 0xE958;
    else if (addr >= 0xE956) res = 0xE956;
    else if (addr >= 0xE94D) res = 0xE94D;
    else if (addr >= 0xE922) res = 0xE922;
    else if (addr >= 0xE918) res = 0xE918;
    else if (addr >= 0xE8FF) res = 0xE8FF;
    else if (addr >= 0xE8F6) res = 0xE8F6;
    else if (addr >= 0xE8EA) res = 0xE8EA;
    else if (addr >= 0xE8CD) res = 0xE8CD;
    else if (addr >= 0xE8CA) res = 0xE8CA;
    else if (addr >= 0xE8C2) res = 0xE8C2;
    else if (addr >= 0xE8B7) res = 0xE8B7;
    else if (addr >= 0xE8B3) res = 0xE8B3;
    else if (addr >= 0xE8B0) res = 0xE8B0;
    else if (addr >= 0xE8A5) res = 0xE8A5;
    else if (addr >= 0xE8A1) res = 0xE8A1;
    else if (addr >= 0xE89E) res = 0xE89E;
    else if (addr >= 0xE888) res = 0xE888;
    else if (addr >= 0xE880) res = 0xE880;
    else if (addr >= 0xE87C) res = 0xE87C;
    else if (addr >= 0xE871) res = 0xE871;
    else if (addr >= 0xE86A) res = 0xE86A;
    else if (addr >= 0xE864) res = 0xE864;
    else if (addr >= 0xE854) res = 0xE854;
    else if (addr >= 0xE84C) res = 0xE84C;
    else if (addr >= 0xE847) res = 0xE847;
    else if (addr >= 0xE832) res = 0xE832;
    else if (addr >= 0xE82D) res = 0xE82D;
    else if (addr >= 0xE829) res = 0xE829;
    else if (addr >= 0xE826) res = 0xE826;
    else if (addr >= 0xE80A) res = 0xE80A;
    else if (addr >= 0xE805) res = 0xE805;
    else if (addr >= 0xE7FE) res = 0xE7FE;
    else if (addr >= 0xE7EA) res = 0xE7EA;
    else if (addr >= 0xE7CE) res = 0xE7CE;
    else if (addr >= 0xE7CB) res = 0xE7CB;
    else if (addr >= 0xE7C8) res = 0xE7C8;
    else if (addr >= 0xE7C0) res = 0xE7C0;
    else if (addr >= 0xE7AD) res = 0xE7AD;
    else if (addr >= 0xE7AA) res = 0xE7AA;
    else if (addr >= 0xE7A8) res = 0xE7A8;
    else if (addr >= 0xE792) res = 0xE792;
    else if (addr >= 0xE78B) res = 0xE78B;
    else if (addr >= 0xE785) res = 0xE785;
    else if (addr >= 0xE782) res = 0xE782;
    else if (addr >= 0xE77E) res = 0xE77E;
    else if (addr >= 0xE773) res = 0xE773;
    else if (addr >= 0xE762) res = 0xE762;
    else if (addr >= 0xE75F) res = 0xE75F;
    else if (addr >= 0xE759) res = 0xE759;
    else if (addr >= 0xE74C) res = 0xE74C;
    else if (addr >= 0xE745) res = 0xE745;
    else if (addr >= 0xE73F) res = 0xE73F;
    else if (addr >= 0xE73D) res = 0xE73D;
    else if (addr >= 0xE731) res = 0xE731;
    else if (addr >= 0xE716) res = 0xE716;
    else if (addr >= 0xE70B) res = 0xE70B;
    else if (addr >= 0xE701) res = 0xE701;
    else if (addr >= 0xE700) res = 0xE700;
    else if (addr >= 0xE6F7) res = 0xE6F7;
    else if (addr >= 0xE6DA) res = 0xE6DA;
    else if (addr >= 0xE6B0) res = 0xE6B0;
    else if (addr >= 0xE6A8) res = 0xE6A8;
    else if (addr >= 0xE69F) res = 0xE69F;
    else if (addr >= 0xE699) res = 0xE699;
    else if (addr >= 0xE697) res = 0xE697;
    else if (addr >= 0xE693) res = 0xE693;
    else if (addr >= 0xE691) res = 0xE691;
    else if (addr >= 0xE690) res = 0xE690;
    else if (addr >= 0xE684) res = 0xE684;
    else if (addr >= 0xE682) res = 0xE682;
    else if (addr >= 0xE674) res = 0xE674;
    else if (addr >= 0xE672) res = 0xE672;
    else if (addr >= 0xE66F) res = 0xE66F;
    else if (addr >= 0xE65D) res = 0xE65D;
    else if (addr >= 0xE654) res = 0xE654;
    else if (addr >= 0xE650) res = 0xE650;
    else if (addr >= 0xE64A) res = 0xE64A;
    else if (addr >= 0xE640) res = 0xE640;
    else if (addr >= 0xE63A) res = 0xE63A;
    else if (addr >= 0xE632) res = 0xE632;
    else if (addr >= 0xE60F) res = 0xE60F;
    else if (addr >= 0xE606) res = 0xE606;
    else if (addr >= 0xE5FE) res = 0xE5FE;
    else if (addr >= 0xE5F3) res = 0xE5F3;
    else if (addr >= 0xE5E7) res = 0xE5E7;
    else if (addr >= 0xE5CA) res = 0xE5CA;
    else if (addr >= 0xE5B9) res = 0xE5B9;
    else if (addr >= 0xE5B4) res = 0xE5B4;
    else if (addr >= 0xE5AA) res = 0xE5AA;
    else if (addr >= 0xE5A8) res = 0xE5A8;
    else if (addr >= 0xE5A0) res = 0xE5A0;
    else if (addr >= 0xE59A) res = 0xE59A;
    else if (addr >= 0xE598) res = 0xE598;
    else if (addr >= 0xE591) res = 0xE591;
    else if (addr >= 0xE582) res = 0xE582;
    else if (addr >= 0xE57C) res = 0xE57C;
    else if (addr >= 0xE570) res = 0xE570;
    else if (addr >= 0xE566) res = 0xE566;
    else if (addr >= 0xE560) res = 0xE560;
    else if (addr >= 0xE513) res = 0xE513;
    else if (addr >= 0xE50A) res = 0xE50A;
    else if (addr >= 0xE505) res = 0xE505;
    else if (addr >= 0xE500) res = 0xE500;
    return res;
}

static char* _get_c64_routine_name(uint16_t addr) {
    switch (addr) {
        case 0xE500: return "IOBASE"; break;
        case 0xE505: return "SCRORG"; break;
        case 0xE50A: return "PLOT"; break;
        case 0xE513: return "PLOT10"; break;
        case 0xE560: return "CLEAR1"; break;
        case 0xE566: return "NXTD"; break;
        case 0xE570: return "FNDSTR"; break;
        case 0xE57C: return "STOK"; break;
        case 0xE582: return "FNDEND"; break;
        case 0xE591: return "FINPUT"; break;
        case 0xE598: return "FINPUX"; break;
        case 0xE59A: return "VPAN"; break;
        case 0xE5A0: return "PANIC"; break;
        case 0xE5A8: return "INITV"; break;
        case 0xE5AA: return "PX4"; break;
        case 0xE5B4: return "LP2"; break;
        case 0xE5B9: return "LP1"; break;
        case 0xE5CA: return "LOOP4"; break;
        case 0xE5E7: return "LP21"; break;
        case 0xE5F3: return "LP23"; break;
        case 0xE5FE: return "LP22"; break;
        case 0xE606: return "CLP5"; break;
        case 0xE60F: return "CLP6"; break;
        case 0xE632: return "LOOP5"; break;
        case 0xE63A: return "LOP5"; break;
        case 0xE640: return "LOP51"; break;
        case 0xE64A: return "LOP54"; break;
        case 0xE650: return "LOP52"; break;
        case 0xE654: return "LOP53"; break;
        case 0xE65D: return "CLP2"; break;
        case 0xE66F: return "CLP2A"; break;
        case 0xE672: return "CLP21"; break;
        case 0xE674: return "CLP1"; break;
        case 0xE682: return "CLP7"; break;
        case 0xE684: return "QTSWC"; break;
        case 0xE690: return "QTSWL"; break;
        case 0xE691: return "NXT33"; break;
        case 0xE693: return "NXT3"; break;
        case 0xE697: return "NC3"; break;
        case 0xE699: return "NVS"; break;
        case 0xE69F: return "NVS1"; break;
        case 0xE6A8: return "LOOP2"; break;
        case 0xE6B0: return "LOP2"; break;
        case 0xE6DA: return "WLOG30"; break;
        case 0xE6F7: return "WLOG10"; break;
        case 0xE700: return "WLGRTS"; break;
        case 0xE701: return "BKLN"; break;
        case 0xE70B: return "BKLN1"; break;
        case 0xE716: return "PRT"; break;
        case 0xE731: return "NJT1"; break;
        case 0xE73D: return "NJT8"; break;
        case 0xE73F: return "NJT9"; break;
        case 0xE745: return "NTCN"; break;
        case 0xE74C: return "CNC3X"; break;
        case 0xE759: return "BAK1UP"; break;
        case 0xE75F: return "BK1"; break;
        case 0xE762: return "BK15"; break;
        case 0xE773: return "BK2"; break;
        case 0xE77E: return "NTCN1"; break;
        case 0xE782: return "CNC3"; break;
        case 0xE785: return "NC3W"; break;
        case 0xE78B: return "NC1"; break;
        case 0xE792: return "NC2"; break;
        case 0xE7A8: return "JPL4"; break;
        case 0xE7AA: return "NCZ2"; break;
        case 0xE7AD: return "NCX2"; break;
        case 0xE7C0: return "CURS10"; break;
        case 0xE7C8: return "GOTDWN"; break;
        case 0xE7CB: return "JPL3"; break;
        case 0xE7CE: return "COLR1"; break;
        case 0xE7EA: return "UP5"; break;
        case 0xE7FE: return "INS3"; break;
        case 0xE805: return "INS1"; break;
        case 0xE80A: return "INS2"; break;
        case 0xE826: return "INSEXT"; break;
        case 0xE829: return "UP9"; break;
        case 0xE82D: return "UP6"; break;
        case 0xE832: return "UP2"; break;
        case 0xE847: return "UPALIN"; break;
        case 0xE84C: return "NXT2"; break;
        case 0xE854: return "NXT6"; break;
        case 0xE864: return "BAKBAK"; break;
        case 0xE86A: return "NXT61"; break;
        case 0xE871: return "JPL2"; break;
        case 0xE87C: return "NXLN"; break;
        case 0xE880: return "NXLN2"; break;
        case 0xE888: return "NXLN1"; break;
        case 0xE89E: return "JPL5"; break;
        case 0xE8A1: return "CHKBAK"; break;
        case 0xE8A5: return "CHKLUP"; break;
        case 0xE8B0: return "BACK"; break;
        case 0xE8B3: return "CHKDWN"; break;
        case 0xE8B7: return "DWNCHK"; break;
        case 0xE8C2: return "DNLINE"; break;
        case 0xE8CA: return "DWNBYE"; break;
        case 0xE8CD: return "CHK1A"; break;
        case 0xE8EA: return "SCROL"; break;
        case 0xE8F6: return "SCRO0"; break;
        case 0xE8FF: return "SCR10"; break;
        case 0xE918: return "SCRL5"; break;
        case 0xE922: return "SCRL3"; break;
        case 0xE94D: return "MLP4"; break;
        case 0xE956: return "MLP42"; break;
        case 0xE958: return "PULIND"; break;
        case 0xE967: return "BMT1"; break;
        case 0xE96C: return "BMT2"; break;
        case 0xE981: return "NEWLX"; break;
        case 0xE98F: return "SCD10"; break;
        case 0xE9BA: return "SCRD19"; break;
        case 0xE9F0: return "SETPNT"; break;
        case 0xE9FF: return "CLRLN"; break;
        case 0xEA07: return "CLR10"; break;
        case 0xEA13: return "DSPP"; break;
        case 0xEA1C: return "DSPP2"; break;
        case 0xEA24: return "SCOLOR"; break;
        case 0xEA31: return "KEY"; break;
        case 0xEA3E: return "REPDO"; break;
        case 0xEA5C: return "KEY5"; break;
        case 0xEA61: return "KEY4"; break;
        case 0xEA71: return "KEY3"; break;
        case 0xEA7B: return "KL2"; break;
        case 0xEA7E: return "KPREND"; break;
        case 0xEA87: return "SCNKEY"; break;
        case 0xEAA8: return "SCN20"; break;
        case 0xEAAB: return "SCN22"; break;
        case 0xEAB3: return "SCN30"; break;
        case 0xEACB: return "CKUT"; break;
        case 0xEACC: return "CKIT"; break;
        case 0xEADC: return "CKIT1"; break;
        case 0xEAE0: return "REKEY"; break;
        case 0xEAF0: return "RPT10"; break;
        case 0xEAFB: return "SCNOUT"; break;
        case 0xEB0D: return "RPT20"; break;
        case 0xEB17: return "RPT40"; break;
        case 0xEB30: return "CKIT3"; break;
        case 0xEB42: return "SCNRTS"; break;
        case 0xEB59: return "SWITCH"; break;
        case 0xEC58: return "ULSET"; break;
        case 0xEC5B: return "OUTHRE"; break;
        case 0xEC72: return "LEXIT"; break;
        case 0xECE7: return "RUNTB"; break;
        case 0xED09: return "TALK"; break;
        case 0xED0C: return "LISTN"; break;
        case 0xED11: return "LIST1"; break;
        case 0xED20: return "LIST2"; break;
        case 0xED2E: return "LIST5"; break;
        case 0xED36: return "ISOURA"; break;
        case 0xED40: return "ISOUR"; break;
        case 0xED50: return "ISR02"; break;
        case 0xED55: return "ISR03"; break;
        case 0xED5A: return "NOEOI"; break; // Wait for DATA low
        case 0xED66: return "ISR01"; break;
        case 0xED7A: return "ISRHI"; break;
        case 0xED7D: return "ISRCLK"; break;
        case 0xED9F: return "ISR04"; break;
        case 0xEDAD: return "NODEV"; break;
        case 0xEDAF: return "FRMERR"; break;
        case 0xEDB2: return "CSBERR"; break;
        case 0xEDB9: return "SECND"; break;
        case 0xEDBE: return "SCATN"; break;
        case 0xEDC7: return "TKSA"; break;
        case 0xEDD6: return "TKATN1"; break;
        case 0xEDDD: return "CIOUT"; break;
        case 0xEDE6: return "CI2"; break;
        case 0xEDEB: return "CI4"; break;
        case 0xEDEF: return "UNTLK"; break;
        case 0xEDFE: return "UNLSN"; break;
        case 0xEE03: return "DLABYE"; break;
        case 0xEE06: return "DLADLH"; break;
        case 0xEE09: return "DLAD00"; break;
        case 0xEE1B: return "ACP00A"; break;
        case 0xEE30: return "ACP00"; break;
        case 0xEE3E: return "ACP00B"; break;
        case 0xEE47: return "ACP00C"; break;
        case 0xEE56: return "ACP01"; break;
        case 0xEE5A: return "ACP03"; break;
        case 0xEE67: return "ACP03A"; break;
        case 0xEE80: return "ACP04"; break;
        case 0xEEA9: return "DEBPIA"; break;
        case 0xEEB6: return "W1MS1"; break;
        case 0xEEBB: return "RSTRAB"; break;
        case 0xEEC8: return "RST005"; break;
        case 0xEED1: return "RSTEXT"; break;
        case 0xEED7: return "RST010"; break;
        case 0xEEE6: return "RSWEXT"; break;
        case 0xEEE7: return "RSPEXT"; break;
        case 0xEEF6: return "RST030"; break;
        case 0xEEFC: return "RST040"; break;
        case 0xEF00: return "RST050"; break;
        case 0xEF06: return "RSTBGN"; break;
        case 0xEF13: return "RST060"; break;
        case 0xEF1C: return "RST070"; break;
        case 0xEF1E: return "RST080"; break;
        case 0xEF2E: return "DSRERR"; break;
        case 0xEF31: return "CTSERR"; break;
        case 0xEF39: return "RSODNE"; break;
        case 0xEF3B: return "OENABL"; break;
        case 0xEF4A: return "BITCNT"; break;
        case 0xEF54: return "BIT010"; break;
        case 0xEF58: return "BIT020"; break;
        case 0xEF59: return "RSRCVR"; break;
        case 0xEF6D: return "RSREXT"; break;
        case 0xEF6E: return "RSR018"; break;
        case 0xEF70: return "RSR020"; break;
        case 0xEF7E: return "RSRABL"; break;
        case 0xEF8B: return "RSRSXT"; break;
        case 0xEF90: return "RSRTRT"; break;
        case 0xEF97: return "RSR030"; break;
        case 0xEFA9: return "RSR031"; break;
        case 0xEFB1: return "RSR032"; break;
        case 0xEFC5: return "RSR050"; break;
        case 0xEFCA: return "RECERR"; break;
        case 0xEFCD: return "BREAKE"; break;
        case 0xEFD0: return "FRAMEE"; break;
        case 0xEFD2: return "ERR232"; break;
        case 0xEFDB: return "RSR060"; break;
        case 0xEFE1: return "CKO232"; break;
        case 0xEFF2: return "CKO020"; break;
        case 0xEFF9: return "CKO030"; break;
        case 0xF006: return "CKO040"; break;
        case 0xF00D: return "CKDSRX"; break;
        case 0xF012: return "CKO100"; break;
        case 0xF014: return "BSOBAD"; break;
        case 0xF017: return "BSO232"; break;
        case 0xF028: return "BSO100"; break;
        case 0xF02E: return "BSO110"; break;
        case 0xF04C: return "BSO120"; break;
        case 0xF04D: return "CKI232"; break;
        case 0xF062: return "CKI010"; break;
        case 0xF070: return "CKI020"; break;
        case 0xF077: return "CKI080"; break;
        case 0xF07D: return "CKI100"; break;
        case 0xF084: return "CKI110"; break;
        case 0xF086: return "BSI232"; break;
        case 0xF09C: return "BSI010"; break;
        case 0xF0A4: return "RSP232"; break;
        case 0xF0AA: return "RSPOFF"; break;
        case 0xF0BB: return "RSPOK"; break;
        case 0xF12B: return "SPMSG"; break;
        case 0xF12F: return "MSG"; break;
        case 0xF13C: return "MSG10"; break;
        case 0xF13E: return "NGETIN"; break;
        case 0xF14A: return "GN10"; break;
        case 0xF14E: return "GN232"; break;
        case 0xF155: return "GN20"; break;
        case 0xF157: return "NBASIN"; break;
        case 0xF166: return "BN10"; break;
        case 0xF173: return "BN20"; break;
        case 0xF18D: return "JTG35"; break;
        case 0xF193: return "JTG36"; break;
        case 0xF196: return "JTG37"; break;
        case 0xF199: return "JTGET"; break;
        case 0xF1A9: return "JTG10"; break;
        case 0xF1AD: return "BN30"; break;
        case 0xF1B1: return "BN31"; break;
        case 0xF1B3: return "BN32"; break;
        case 0xF1B4: return "BN33"; break;
        case 0xF1B5: return "BN35"; break;
        case 0xF1B8: return "BN50"; break;
        case 0xF1CA: return "NBSOUT"; break;
        case 0xF1DB: return "BO20"; break;
        case 0xF1DD: return "CASOUT"; break;
        case 0xF1F8: return "JTP10"; break;
        case 0xF1FC: return "RSTOA"; break;
        case 0xF1FD: return "RSTOR"; break;
        case 0xF207: return "RSTOR1"; break;
        case 0xF208: return "BO50"; break;
        case 0xF20E: return "NCHKIN"; break;
        case 0xF216: return "JX310"; break;
        case 0xF22A: return "JX315"; break;
        case 0xF233: return "JX320"; break;
        case 0xF237: return "JX330"; break;
        case 0xF245: return "JX340"; break;
        case 0xF248: return "JX350"; break;
        case 0xF250: return "NCKOUT"; break;
        case 0xF258: return "CK5"; break;
        case 0xF25F: return "CK20"; break;
        case 0xF262: return "CK10"; break;
        case 0xF26F: return "CK15"; break;
        case 0xF275: return "CK30"; break;
        case 0xF279: return "CK40"; break;
        case 0xF286: return "CK50"; break;
        case 0xF289: return "CK60"; break;
        case 0xF291: return "NCLOSE"; break;
        case 0xF298: return "JX050"; break;
        case 0xF2BA: return "CLS010"; break;
        case 0xF2BF: return "CLS020"; break;
        case 0xF2C8: return "JX115"; break;
        case 0xF2E0: return "JX117"; break;
        case 0xF2EE: return "JX120"; break;
        case 0xF2F1: return "JX150"; break;
        case 0xF2F2: return "JXRMV"; break;
        case 0xF30D: return "JX170"; break;
        case 0xF30E: return "JX175"; break;
        case 0xF30F: return "LOOKUP"; break;
        case 0xF314: return "JLTLK"; break;
        case 0xF316: return "JX600"; break;
        case 0xF31F: return "JZ100"; break;
        case 0xF32E: return "JZ101"; break;
        case 0xF32F: return "NCLALL"; break;
        case 0xF333: return "NCLRCH"; break;
        case 0xF33C: return "JX750"; break;
        case 0xF343: return "CLALL2"; break;
        case 0xF34A: return "NOPEN"; break;
        case 0xF351: return "OP98"; break;
        case 0xF359: return "OP100"; break;
        case 0xF362: return "OP110"; break;
        case 0xF384: return "OP150"; break;
        case 0xF38B: return "OP152"; break;
        case 0xF393: return "OP155"; break;
        case 0xF3AC: return "OP160"; break;
        case 0xF3AF: return "OP170"; break;
        case 0xF3B8: return "OP200"; break;
        case 0xF3C2: return "OP171"; break;
        case 0xF3D1: return "OP172"; break;
        case 0xF3D3: return "OP175"; break;
        case 0xF3D4: return "OP180"; break;
        case 0xF3D5: return "OPENI"; break;
        case 0xF3F6: return "OP35"; break;
        case 0xF3FC: return "OP40"; break;
        case 0xF406: return "OP45"; break;
        case 0xF409: return "OPN232"; break;
        case 0xF40F: return "OPN020"; break;
        case 0xF41D: return "OPN025"; break;
        case 0xF43A: return "OPN026"; break;
        case 0xF440: return "OPN027"; break;
        case 0xF446: return "OPN028"; break;
        case 0xF44D: return "OPN030"; break;
        case 0xF45C: return "OPN050"; break;
        case 0xF468: return "OPN055"; break;
        case 0xF474: return "OPN060"; break;
        case 0xF47D: return "MEMTCF"; break;
        case 0xF483: return "CLN232"; break;
        case 0xF49E: return "LOADSP"; break;
        case 0xF4A2: return "LOAD"; break;
        case 0xF4A5: return "NLOAD"; break;
        case 0xF4AF: return "LD10"; break;
        case 0xF4B2: return "LD20"; break;
        case 0xF4BF: return "LD25"; break;
        case 0xF4F0: return "LD30"; break;
        case 0xF4F3: return "LD40"; break;
        case 0xF501: return "LD45"; break;
        case 0xF51C: return "LD50"; break;
        case 0xF51E: return "LD60"; break;
        case 0xF524: return "LD64"; break;
        case 0xF530: return "LD90"; break;
        case 0xF533: return "LD100"; break;
        case 0xF539: return "LD102"; break;
        case 0xF541: return "LD104"; break;
        case 0xF549: return "LD112"; break;
        case 0xF556: return "LD150"; break;
        case 0xF55D: return "LD170"; break;
        case 0xF56C: return "LD177"; break;
        case 0xF579: return "LD178"; break;
        case 0xF57D: return "LD179"; break;
        case 0xF5A9: return "LD180"; break;
        case 0xF5AE: return "LD190"; break;
        case 0xF5AF: return "LUKING"; break;
        case 0xF5C1: return "OUTFN"; break;
        case 0xF5C7: return "LD110"; break;
        case 0xF5D1: return "LD115"; break;
        case 0xF5D2: return "LODING"; break;
        case 0xF5DA: return "LD410"; break;
        case 0xF5DD: return "SAVESP"; break;
        case 0xF5EA: return "SAVE"; break;
        case 0xF5ED: return "NSAVE"; break;
        case 0xF5F1: return "SV10"; break;
        case 0xF5F4: return "SV20"; break;
        case 0xF605: return "SV25"; break;
        case 0xF624: return "SV30"; break;
        case 0xF633: return "BREAK"; break;
        case 0xF63A: return "SV40"; break;
        case 0xF63F: return "SV50"; break;
        case 0xF642: return "CLSEI"; break;
        case 0xF654: return "CUNLSN"; break;
        case 0xF657: return "CLSEI2"; break;
        case 0xF659: return "SV100"; break;
        case 0xF65F: return "SV102"; break;
        case 0xF66C: return "SV105"; break;
        case 0xF676: return "SV106"; break;
        case 0xF68D: return "SV110"; break;
        case 0xF68E: return "SV115"; break;
        case 0xF68F: return "SAVING"; break;
        case 0xF69B: return "UDTIM"; break;
        case 0xF69D: return "UD20"; break;
        case 0xF6A7: return "UD30"; break;
        case 0xF6BC: return "UD60"; break;
        case 0xF6CC: return "UD70"; break;
        case 0xF6DA: return "UD80"; break;
        case 0xF6DC: return "UD90"; break;
        case 0xF6DD: return "RDTIM"; break;
        case 0xF6E4: return "SETTIM"; break;
        case 0xF6ED: return "NSTOP"; break;
        case 0xF6FA: return "STOP2"; break;
        case 0xF6FB: return "ERROR1"; break;
        case 0xF6FE: return "ERROR2"; break;
        case 0xF701: return "ERROR3"; break;
        case 0xF704: return "ERROR4"; break;
        case 0xF707: return "ERROR5"; break;
        case 0xF70A: return "ERROR6"; break;
        case 0xF70D: return "ERROR7"; break;
        case 0xF710: return "ERROR8"; break;
        case 0xF713: return "ERROR9"; break;
        case 0xF729: return "EREXIT"; break;
        case 0xF72C: return "FAH"; break;
        case 0xF74B: return "FAH50"; break;
        case 0xF757: return "FAH55"; break;
        case 0xF761: return "FAH56"; break;
        case 0xF767: return "FAH45"; break;
        case 0xF769: return "FAH40"; break;
        case 0xF76A: return "TAPEH"; break;
        case 0xF781: return "BLNK2"; break;
        case 0xF7A5: return "TH20"; break;
        case 0xF7B7: return "TH30"; break;
        case 0xF7CF: return "TH40"; break;
        case 0xF7D0: return "ZZZ"; break;
        case 0xF7D7: return "LDAD1"; break;
        case 0xF7EA: return "FAF"; break;
        case 0xF7F7: return "FAF20"; break;
        case 0xF80B: return "FAF30"; break;
        case 0xF80C: return "FAF40"; break;
        case 0xF80D: return "JTP20"; break;
        case 0xF817: return "CSTE1"; break;
        case 0xF81E: return "CS30"; break;
        case 0xF821: return "CS40"; break;
        case 0xF82E: return "CS10"; break;
        case 0xF836: return "CS25"; break;
        case 0xF838: return "CSTE2"; break;
        case 0xF841: return "RBLK"; break;
        case 0xF84A: return "TRD"; break;
        case 0xF864: return "WBLK"; break;
        case 0xF867: return "TWRT"; break;
        case 0xF86B: return "TWRT2"; break;
        case 0xF86E: return "TWRT3"; break;
        case 0xF875: return "TAPE"; break;
        case 0xF8B5: return "TP32"; break;
        case 0xF8B7: return "TP35"; break;
        case 0xF8BE: return "TP40"; break;
        case 0xF8D0: return "TSTOP"; break;
        case 0xF8DC: return "STOP3"; break;
        case 0xF8E1: return "STOP4"; break;
        case 0xF8E2: return "STT1"; break;
        case 0xF8F7: return "STT2"; break;
        case 0xF8FE: return "STT3"; break;
        case 0xF92A: return "STT4"; break;
        case 0xF92C: return "READ"; break;
        case 0xF969: return "RJDJ"; break;
        case 0xF988: return "JRAD2"; break;
        case 0xF98B: return "SRER"; break;
        case 0xF993: return "RADX2"; break;
        case 0xF997: return "RADL"; break;
        case 0xF999: return "RAD5"; break;
        case 0xF9AC: return "RDBK"; break;
        case 0xF9BC: return "RADKX"; break;
        case 0xF9C9: return "RADP"; break;
        case 0xF9D2: return "RADBK"; break;
        case 0xF9D5: return "RAD3"; break;
        case 0xF9DE: return "ROUT2"; break;
        case 0xF9E0: return "ROUT1"; break;
        case 0xF9F7: return "RAD4"; break;
        case 0xFA10: return "RAD2"; break;
        case 0xFA18: return "RAD2Y"; break;
        case 0xFA1F: return "RAD2X"; break;
        case 0xFA44: return "RADQ2"; break;
        case 0xFA53: return "RADK"; break;
        case 0xFA5D: return "RDBK2"; break;
        case 0xFA60: return "RADJ"; break;
        case 0xFA70: return "RD15"; break;
        case 0xFA86: return "RD12"; break;
        case 0xFA8A: return "RD10"; break;
        case 0xFA8D: return "RD20"; break;
        case 0xFAA3: return "RD22"; break;
        case 0xFAA9: return "RD200"; break;
        case 0xFABA: return "RD40"; break;
        case 0xFAC0: return "RD60"; break;
        case 0xFACE: return "RD70"; break;
        case 0xFAEB: return "RD80"; break;
        case 0xFB08: return "RD58"; break;
        case 0xFB2F: return "RD52"; break;
        case 0xFB33: return "RD55"; break;
        case 0xFB3A: return "RD59"; break;
        case 0xFB43: return "RD90"; break;
        case 0xFB48: return "RD160"; break;
        case 0xFB4A: return "RD161"; break;
        case 0xFB5C: return "RD167"; break;
        case 0xFB68: return "RD175"; break;
        case 0xFB72: return "VPRTY"; break;
        case 0xFB8B: return "RD180"; break;
        case 0xFB8E: return "RD300"; break;
        case 0xFB97: return "NEWCH"; break;
        case 0xFBA6: return "WRITE"; break;
        case 0xFBAD: return "WRTW"; break;
        case 0xFBAF: return "WRT1"; break;
        case 0xFBB1: return "WRTX"; break;
        case 0xFBC8: return "WRTL3"; break;
        case 0xFBCD: return "WRTN"; break;
        case 0xFBE3: return "WRTN1"; break;
        case 0xFBF0: return "WRTN2"; break;
        case 0xFC09: return "WRT3"; break;
        case 0xFC0C: return "WRT2"; break;
        case 0xFC16: return "WRTS"; break;
        case 0xFC22: return "WRTS1"; break;
        case 0xFC2C: return "WRT61"; break;
        case 0xFC30: return "WRT6"; break;
        case 0xFC3F: return "WRT7"; break;
        case 0xFC4E: return "WRT4"; break;
        case 0xFC54: return "WRTBK"; break;
        case 0xFC57: return "WRNC"; break;
        case 0xFC5E: return "WREND"; break;
        case 0xFC6A: return "WRTZ"; break;
        case 0xFC93: return "TNIF"; break;
        case 0xFCB6: return "TNIQ"; break;
        case 0xFCB8: return "STKY"; break;
        case 0xFCBD: return "BSIV"; break;
        case 0xFCCA: return "TNOF"; break;
        case 0xFCD1: return "CMPSTE"; break;
        case 0xFCDB: return "INCSAL"; break;
        case 0xFCE1: return "INCR"; break;
        case 0xFCE2: return "START"; break;
        case 0xFCEF: return "START1"; break;
        case 0xFD02: return "A0INT"; break;
        case 0xFD04: return "A0IN1"; break;
        case 0xFD0F: return "A0IN2"; break;
        case 0xFD10: return "TBLA0R"; break;
        case 0xFD15: return "RESTOR"; break;
        case 0xFD1A: return "VECTOR"; break;
        case 0xFD20: return "MOVOS1"; break;
        case 0xFD27: return "MOVOS2"; break;
        case 0xFD50: return "RAMTAS"; break;
        case 0xFD53: return "RAMTZ0"; break;
        case 0xFD6C: return "RAMTZ1"; break;
        case 0xFD6E: return "RAMTZ2"; break;
        case 0xFD88: return "SIZE"; break;
        case 0xFD9B: return "BSIT"; break;
        case 0xFDA3: return "IOINIT"; break;
        case 0xFDDD: return "IOKEYS"; break;
        case 0xFDEC: return "I0010"; break;
        case 0xFDF3: return "I0020"; break;
        case 0xFDF9: return "SETNAM"; break;
        case 0xFE00: return "SETLFS"; break;
        case 0xFE07: return "READSS"; break;
        case 0xFE18: return "SETMSG"; break;
        case 0xFE1A: return "READST"; break;
        case 0xFE1C: return "UDST"; break;
        case 0xFE21: return "SETTMO"; break;
        case 0xFE25: return "MEMTOP"; break;
        case 0xFE27: return "GETTOP"; break;
        case 0xFE2D: return "SETTOP"; break;
        case 0xFE34: return "MEMBOT"; break;
        case 0xFE3C: return "SETBOT"; break;
        case 0xFE43: return "NMI"; break;
        case 0xFE47: return "NNMI"; break;
        case 0xFE4C: return "NNMI10"; break;
        case 0xFE56: return "NNMI18"; break;
        case 0xFE66: return "TIMB"; break;
        case 0xFE72: return "NNMI20"; break;
        case 0xFE9A: return "NNMI22"; break;
        case 0xFE9D: return "NNMI25"; break;
        case 0xFEA3: return "NNMI30"; break;
        case 0xFEAE: return "NNMI40"; break;
        case 0xFEB6: return "NMIRTI"; break;
        case 0xFEBC: return "PREND"; break;
        case 0xFED6: return "T2NMI"; break;
        case 0xFF2E: return "POPEN"; break;
        case 0xFF43: return "SIMIRQ"; break;
        case 0xFF48: return "PULS"; break;
        case 0xFF58: return "PULS1"; break;
        case 0xFFC0: return "OPEN"; break;
        case 0xFFC3: return "CLOSE"; break;
        case 0xFFC6: return "CHKIN"; break;
        case 0xFFC9: return "CKOUT"; break;
        case 0xFFCC: return "CLRCH"; break;
        case 0xFFCF: return "BASIN"; break;
        case 0xFFD2: return "BSOUT"; break;
        case 0xFFE1: return "STOP"; break;
        case 0xFFE4: return "GETIN"; break;
        case 0xFFE7: return "CLALL"; break;
        case 0xFFED: return "JSCROG"; break;
        case 0xFFF0: return "JPLOT"; break;
        case 0xFFF3: return "JIOBAS"; break;
        default:
            return NULL;
    }
}

// char* last_routine = NULL;
static void _c64_debug_out_processor_pc(c64_t* sys, uint64_t pins) {
    if (pins & M6502_SYNC) {
        uint16_t cpu_pc = m6502_pc(&sys->cpu);
        uint16_t function_address = _get_c64_nearest_routine_address(cpu_pc);
        uint16_t address_diff = cpu_pc - function_address;
        char* function_name = _get_c64_routine_name(function_address);
        if (function_name == NULL) {
            function_name = "";
        }
        if (sys->debug_file) {
            char iec_status[5];
            char local_iec_status[5];
            iec_get_status_text(&sys->iec_bus, iec_status);
            iec_get_device_status_text(sys->iec_device, local_iec_status);

            const uint8_t iec_lines = iec_get_signals(&sys->iec_bus, sys->iec_device);
            uint8_t cia2_pa = M6526_GET_PA(sys->cia_2.pins);
            // CIA2 input receives logic HIGH if IEC voltage is high (meaning IEC logic LOW)
            cia2_pa = (cia2_pa & ~(1 << 6)) | (((iec_lines & IECLINE_CLK) == 0) ? (1<<6) : 0);
            cia2_pa = (cia2_pa & ~(1 << 7)) | (((iec_lines & IECLINE_DATA) == 0) ? (1<<7) : 0);

            if (address_diff > 0) {
                fprintf(sys->debug_file, "tick:%10ld\taddr:%04x\tsys:c64 \tport-cia2-a:%02x\tbus-iec:%s\tlocal-iec:%s\tlabel:%s+%x\n", get_world_tick(), cpu_pc, cia2_pa, iec_status, local_iec_status, function_name, address_diff);
            } else {
                fprintf(sys->debug_file, "tick:%10ld\taddr:%04x\tsys:c64 \tport-cia2-a:%02x\tbus-iec:%s\tlocal-iec:%s\tlabel:%s\n", get_world_tick(), cpu_pc, cia2_pa, iec_status, local_iec_status, function_name);
            }
        }
    }
}
#endif

static uint64_t _c64_tick(c64_t* sys, uint64_t pins) {
    static uint16_t last_cpu_address = 0;
#ifdef __IEC_DEBUG
    _c64_debug_out_processor_pc(sys, pins);
#endif
    iec_get_signals(&sys->iec_bus, NULL);
    // tick the CPU
    pins = m6502_tick(&sys->cpu, pins);
    const uint16_t addr = M6502_GET_ADDR(pins);
    const uint8_t iec_lines = iec_get_signals(&sys->iec_bus, sys->iec_device);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(M6502_IRQ|M6502_NMI|M6502_RDY|M6510_AEC);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool cpu_io_access = false;
    bool color_ram_access = false;
    bool mem_access = false;
    uint64_t vic_pins = pins & M6502_PIN_MASK;
    uint64_t cia1_pins = pins & M6502_PIN_MASK;
    uint64_t cia2_pins = pins & M6502_PIN_MASK;
    uint64_t sid_pins = pins & M6502_PIN_MASK;
    if ((pins & (M6502_RDY|M6502_RW)) != (M6502_RDY|M6502_RW)) {
        if (M6510_CHECK_IO(pins)) {
            cpu_io_access = true;
        }
        else {
            if (sys->io_mapped && ((addr & 0xF000) == 0xD000)) {
                if (addr < 0xD400) {
                    // VIC-II (D000..D3FF)
                    vic_pins |= M6569_CS;
                }
                else if (addr < 0xD800) {
                    // SID (D400..D7FF)
                    sid_pins |= M6581_CS;
                }
                else if (addr < 0xDC00) {
                    // read or write the special color Static-RAM bank (D800..DBFF)
                    color_ram_access = true;
                }
                else if (addr < 0xDD00) {
                    // CIA-1 (DC00..DCFF)
                    cia1_pins |= M6526_CS;
                }
                else if (addr < 0xDE00) {
                    // CIA-2 (DD00..DDFF)
                    cia2_pins |= M6526_CS;
                }
            }
            else {
                mem_access = true;
            }
        }
    }
    if (pins & M6502_SYNC) {
        last_cpu_address = addr;
    }

    // tick the SID
    {
        sid_pins = m6581_tick(&sys->sid, sid_pins);
        if (sid_pins & M6581_SAMPLE) {
            // new audio sample ready
            sys->audio.sample_buffer[sys->audio.sample_pos++] = sys->sid.sample;
            if (sys->audio.sample_pos == sys->audio.num_samples) {
                if (sys->audio.callback.func) {
                    sys->audio.callback.func(sys->audio.sample_buffer, sys->audio.num_samples, sys->audio.callback.user_data);
                }
                sys->audio.sample_pos = 0;
            }
        }
        if ((sid_pins & (M6581_CS|M6581_RW)) == (M6581_CS|M6581_RW)) {
            pins = M6502_COPY_DATA(pins, sid_pins);
        }
    }

    /* tick CIA-1:

        In Port A:
            joystick 2 input
        In Port B:
            combined keyboard matrix columns and joystick 1
        Cas Port Read => Flag pin

        Out Port A:
            write keyboard matrix lines

        IRQ pin is connected to the CPU IRQ pin
    */
    {
        // cassette port READ pin is connected to CIA-1 FLAG pin
        const uint8_t pa = ~(sys->kbd_joy2_mask|sys->joy_joy2_mask);
        const uint8_t pb = ~(kbd_scan_columns(&sys->kbd) | sys->kbd_joy1_mask | sys->joy_joy1_mask);
        M6526_SET_PAB(cia1_pins, pa, pb);
        if (sys->cas_port & C64_CASPORT_READ || iec_lines & IECLINE_SRQIN) {
            cia1_pins |= M6526_FLAG;
        }
        cia1_pins = m6526_tick(&sys->cia_1, cia1_pins);
        const uint8_t kbd_lines = ~M6526_GET_PA(cia1_pins);
        kbd_set_active_lines(&sys->kbd, kbd_lines);
        if (cia1_pins & M6502_IRQ) {
            pins |= M6502_IRQ;
        }
        if ((cia1_pins & (M6526_CS|M6526_RW)) == (M6526_CS|M6526_RW)) {
            pins = M6502_COPY_DATA(pins, cia1_pins);
        }
    }

    /* tick CIA-2
        In Port A:
            bits 0..5: output (see cia2_out)
            bit 6: IEC CLK in
            bit 7: IEC DATA in
        In Port B:
            RS232 / user functionality (not implemented)

        Out Port A:
            bits 0..1: VIC-II bank select:
                00: bank 3 C000..FFFF
                01: bank 2 8000..BFFF
                10: bank 1 4000..7FFF
                11: bank 0 0000..3FFF
            bit 2: RS-232 TXD Outout (not implemented)
            bit 3: IEC ATN out
            bit 4: IEC CLK out
            bit 5: IEC DATA out
            bit 6..7: input (see cia2_in)
        Out Port B:
            RS232 / user functionality (not implemented)

        CIA-2 IRQ pin connected to CPU NMI pin
    */
    {
        // handle IEC communication
        uint8_t cia2_pa = M6526_GET_PA(sys->cia_2.pins);
        uint8_t cia2_pb = M6526_GET_PB(sys->cia_2.pins);

        // clear CLK and DATA pins
        cia2_pa &= ~(3 << 6);
        if ((iec_lines & IECLINE_CLK) == 0) {
            cia2_pa |= 1 << 6;
        }
        if ((iec_lines & IECLINE_DATA) == 0) {
            cia2_pa |= 1<< 7;
        }

        M6526_SET_PAB(cia2_pins, cia2_pa, cia2_pb);
        cia2_pins = m6526_tick(&sys->cia_2, cia2_pins);
        sys->vic_bank_select = ((~M6526_GET_PA(cia2_pins))&3)<<14;
        if (cia2_pins & M6502_IRQ) {
            pins |= M6502_NMI;
        }
        if (cia2_pins & M6526_CS) {
            if (cia2_pins & M6526_RW) {
                pins = M6502_COPY_DATA(pins, cia2_pins);
            } else {
                if (addr == 0xdd00) {
                    // write
                    uint8_t write_data = M6502_GET_DATA(cia2_pins);
//                    printf("%ld - c64 - Write CIA2 $DD00 = $%02X - CPU @ $%04X\n", get_world_tick(), write_data, last_cpu_address);
                }
            }
        }
        {
            uint8_t signals = sys->iec_device->signals & ~(IECLINE_ATN | IECLINE_CLK | IECLINE_DATA);
            
            cia2_pa = M6526_GET_PA(cia2_pins);

            if (cia2_pins & M6522_PA3) {
                signals |= IECLINE_ATN;
            }
            if (cia2_pins & M6522_PA4) {
                signals |= IECLINE_CLK;
            }
            if (cia2_pins & M6522_PA5) {
                signals |= IECLINE_DATA;
            }

            if (signals != sys->iec_device->signals) {
                char message_prefix[256];
//                sprintf(message_prefix, "%ld - c64 - write-iec - cpu @ $%04X", get_world_tick(), last_cpu_address);
                sys->iec_device->signals = signals;
// #ifdef __IEC_DEBUG
//                iec_debug_print_device_signals(sys->iec_device, message_prefix);
// #endif
            }
        }
    }

    // the RESTORE key, along with CIA-2 IRQ, is connected to the NMI line,
    if(sys->kbd.scanout_column_masks[8] & 1) {
        pins |= M6502_NMI;
    }

    /* tick the VIC-II display chip:
        - the VIC-II IRQ pin is connected to the CPU IRQ pin and goes
        active when the VIC-II requests a rasterline interrupt
        - the VIC-II BA pin is connected to the CPU RDY pin, and stops
        the CPU on the first CPU read access after BA goes active
        - the VIC-II AEC pin is connected to the CPU AEC pin, currently
        this goes active during a badline, but is not checked
    */
    {
        vic_pins = m6569_tick(&sys->vic, vic_pins);
        pins |= (vic_pins & (M6502_IRQ|M6502_RDY|M6510_AEC));
        if ((vic_pins & (M6569_CS|M6569_RW)) == (M6569_CS|M6569_RW)) {
            pins = M6502_COPY_DATA(pins, vic_pins);
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (cpu_io_access) {
        // ...the integrated IO port in the M6510 CPU at addresses 0 and 1
        pins = m6510_iorq(&sys->cpu, pins);
    }
    else if (color_ram_access) {
        if (pins & M6502_RW) {
            M6502_SET_DATA(pins, sys->color_ram[addr & 0x03FF]);
        }
        else {
            sys->color_ram[addr & 0x03FF] = M6502_GET_DATA(pins);
        }
    }
    else if (mem_access) {
        if (pins & M6502_RW) {
            // memory read
            uint8_t read_data = mem_rd(&sys->mem_cpu, addr);
            M6502_SET_DATA(pins, read_data);
            // FIXME: for debugging purpose only
            if (addr == 0xdd00) {
                printf("%ld - c64 - Read CIA2 $DD00 = $%02X - CPU @ $%04X\n", get_world_tick(), read_data, last_cpu_address);
            }
        }
        else {
            // memory write
            uint8_t write_data = M6502_GET_DATA(pins);
            mem_wr(&sys->mem_cpu, addr, write_data);
            // FIXME: for debugging purpose only
            if (addr == 0xdd00) {
                printf("%ld - c64 - Write CIA2 $DD00 = $%02X - CPU @ $%04X\n", get_world_tick(), write_data, last_cpu_address);
            }
        }
    }

    if (sys->c1530.valid) {
        c1530_tick(&sys->c1530);
    }
    if (sys->c1541.valid) {
        c1541_tick(&sys->c1541);
    }
    return pins;
}

static uint8_t _c64_cpu_port_in(void* user_data) {
    c64_t* sys = (c64_t*) user_data;
    /*
        Input from the integrated M6510 CPU IO port

        bit 4: [in] datasette button status (1: no button pressed)
    */
    uint8_t val = 7;
    if (sys->cas_port & C64_CASPORT_SENSE) {
        val |= (1<<4);
    }
    return val;
}

static void _c64_cpu_port_out(uint8_t data, void* user_data) {
    c64_t* sys = (c64_t*) user_data;
    /*
        Output to the integrated M6510 CPU IO port

        bits 0..2:  [out] memory configuration

        bit 3: [out] datasette output signal level
        bit 5: [out] datasette motor control (1: motor off)
    */
    if (data & (1<<5)) {
        /* motor off */
        sys->cas_port |= C64_CASPORT_MOTOR;
    }
    else {
        /* motor on */
        sys->cas_port &= ~C64_CASPORT_MOTOR;
    }
    /* only update memory configuration if the relevant bits have changed */
    bool need_mem_update = 0 != ((sys->cpu_port ^ data) & 7);
    sys->cpu_port = data;
    if (need_mem_update) {
        _c64_update_memory_map(sys);
    }
}

static uint16_t _c64_vic_fetch(uint16_t addr, void* user_data) {
    c64_t* sys = (c64_t*) user_data;
    /*
        Fetch data into the VIC-II.

        The VIC-II has a 14-bit address bus and 12-bit data bus, and
        has a different memory mapping than the CPU (that's why it
        goes through the mem_vic pagetable):
            - a full 16-bit address is formed by taking the address bits
              14 and 15 from the value written to CIA-1 port A
            - the lower 8 bits of the VIC-II data bus are connected
              to the shared system data bus, this is used to read
              character mask and pixel data
            - the upper 4 bits of the VIC-II data bus are hardwired to the
              static color RAM
    */
    addr |= sys->vic_bank_select;
    uint16_t data = (sys->color_ram[addr & 0x03FF]<<8) | mem_rd(&sys->mem_vic, addr);
    return data;
}

static void _c64_update_memory_map(c64_t* sys) {
    sys->io_mapped = false;
    uint8_t* read_ptr;
    // shortcut if HIRAM and LORAM is 0, everything is RAM
    if ((sys->cpu_port & (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM)) == 0) {
        mem_map_ram(&sys->mem_cpu, 0, 0xA000, 0x6000, sys->ram+0xA000);
    }
    else {
        // A000..BFFF is either RAM-behind-BASIC-ROM or RAM
        if ((sys->cpu_port & (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM)) == (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM)) {
            read_ptr = sys->rom_basic;
        }
        else {
            read_ptr = sys->ram + 0xA000;
        }
        mem_map_rw(&sys->mem_cpu, 0, 0xA000, 0x2000, read_ptr, sys->ram+0xA000);

        // E000..FFFF is either RAM-behind-KERNAL-ROM or RAM
        if (sys->cpu_port & C64_CPUPORT_HIRAM) {
            read_ptr = sys->rom_kernal;
        }
        else {
            read_ptr = sys->ram + 0xE000;
        }
        mem_map_rw(&sys->mem_cpu, 0, 0xE000, 0x2000, read_ptr, sys->ram+0xE000);

        // D000..DFFF can be Char-ROM or I/O
        if  (sys->cpu_port & C64_CPUPORT_CHAREN) {
            sys->io_mapped = true;
        }
        else {
            mem_map_rw(&sys->mem_cpu, 0, 0xD000, 0x1000, sys->rom_char, sys->ram+0xD000);
        }
    }
}

static void _c64_init_memory_map(c64_t* sys) {
    // seperate memory mapping for CPU and VIC-II
    mem_init(&sys->mem_cpu);
    mem_init(&sys->mem_vic);

    /*
        the C64 has a weird RAM init pattern of 64 bytes 00 and 64 bytes FF
        alternating, probably with some randomness sprinkled in
        (see this thread: http://csdb.dk/forums/?roomid=11&topicid=116800&firstpost=2)
        this is important at least for the value of the 'ghost byte' at 0x3FFF,
        which is 0xFF
    */
    size_t i;
    for (i = 0; i < (1<<16);) {
        for (size_t j = 0; j < 64; j++, i++) {
            sys->ram[i] = 0x00;
        }
        for (size_t j = 0; j < 64; j++, i++) {
            sys->ram[i] = 0xFF;
        }
    }
    CHIPS_ASSERT(i == 0x10000);

    /* setup the initial CPU memory map
       0000..9FFF and C000.CFFF is always RAM
    */
    mem_map_ram(&sys->mem_cpu, 0, 0x0000, 0xA000, sys->ram);
    mem_map_ram(&sys->mem_cpu, 0, 0xC000, 0x1000, sys->ram+0xC000);
    // A000..BFFF, D000..DFFF and E000..FFFF are configurable
    _c64_update_memory_map(sys);

    /* setup the separate VIC-II memory map (64 KByte RAM) overlayed with
       character ROMS at 0x1000.0x1FFF and 0x9000..0x9FFF
    */
    mem_map_ram(&sys->mem_vic, 1, 0x0000, 0x10000, sys->ram);
    mem_map_rom(&sys->mem_vic, 0, 0x1000, 0x1000, sys->rom_char);
    mem_map_rom(&sys->mem_vic, 0, 0x9000, 0x1000, sys->rom_char);
}

static void _c64_init_key_map(c64_t* sys) {
    /*
        http://sta.c64.org/cbm64kbdlay.html
        http://sta.c64.org/cbm64petkey.html
    */
    kbd_init(&sys->kbd, 1);

    const char* keymap =
        // no shift
        "        "
        "3WA4ZSE "
        "5RD6CFTX"
        "7YG8BHUV"
        "9IJ0MKON"
        "+PL-.:@,"
        "~*;  = /"  // ~ is actually the British Pound sign
        "1  2  Q "

        // shift
        "        "
        "#wa$zse "
        "%rd&cftx"
        "'yg(bhuv"
        ")ij0mkon"
        " pl >[ <"
        "$ ]    ?"
        "!  \"  q ";
    CHIPS_ASSERT(strlen(keymap) == 128);
    // shift is column 7, line 1
    kbd_register_modifier(&sys->kbd, 0, 7, 1);
    // ctrl is column 2, line 7
    kbd_register_modifier(&sys->kbd, 1, 2, 7);
    for (int shift = 0; shift < 2; shift++) {
        for (int column = 0; column < 8; column++) {
            for (int line = 0; line < 8; line++) {
                int c = keymap[shift*64 + line*8 + column];
                if (c != 0x20) {
                    kbd_register_key(&sys->kbd, c, column, line, shift?(1<<0):0);
                }
            }
        }
    }

    // special keys
    kbd_register_key(&sys->kbd, C64_KEY_SPACE   , 4, 7, 0);    // space
    kbd_register_key(&sys->kbd, C64_KEY_CSRLEFT , 2, 0, 1);    // cursor left (shift+cursor right)
    kbd_register_key(&sys->kbd, C64_KEY_CSRRIGHT, 2, 0, 0);    // cursor right
    kbd_register_key(&sys->kbd, C64_KEY_CSRDOWN , 7, 0, 0);    // cursor down
    kbd_register_key(&sys->kbd, C64_KEY_CSRUP   , 7, 0, 1);    // cursor up (shift+cursor down)
    kbd_register_key(&sys->kbd, C64_KEY_DEL     , 0, 0, 0);    // delete
    kbd_register_key(&sys->kbd, C64_KEY_INST    , 0, 0, 1);    // inst (shift+del)
    kbd_register_key(&sys->kbd, C64_KEY_HOME    , 3, 6, 0);    // home
    kbd_register_key(&sys->kbd, C64_KEY_CLR     , 3, 6, 1);    // clear (shift+home)
    kbd_register_key(&sys->kbd, C64_KEY_RETURN  , 1, 0, 0);    // return
    kbd_register_key(&sys->kbd, C64_KEY_CTRL    , 2, 7, 0);    // ctrl
    kbd_register_key(&sys->kbd, C64_KEY_CBM     , 5, 7, 0);    // C= commodore key
    kbd_register_key(&sys->kbd, C64_KEY_RESTORE , 0, 8, 0);    // restore (connected to the NMI line)
    kbd_register_key(&sys->kbd, C64_KEY_STOP    , 7, 7, 0);    // run stop
    kbd_register_key(&sys->kbd, C64_KEY_RUN     , 7, 7, 1);    // run (shift+run stop)
    kbd_register_key(&sys->kbd, C64_KEY_LEFT    , 1, 7, 0);    // left arrow symbol
    kbd_register_key(&sys->kbd, C64_KEY_F1      , 4, 0, 0);    // F1
    kbd_register_key(&sys->kbd, C64_KEY_F2      , 4, 0, 1);    // F2
    kbd_register_key(&sys->kbd, C64_KEY_F3      , 5, 0, 0);    // F3
    kbd_register_key(&sys->kbd, C64_KEY_F4      , 5, 0, 1);    // F4
    kbd_register_key(&sys->kbd, C64_KEY_F5      , 6, 0, 0);    // F5
    kbd_register_key(&sys->kbd, C64_KEY_F6      , 6, 0, 1);    // F6
    kbd_register_key(&sys->kbd, C64_KEY_F7      , 3, 0, 0);    // F7
    kbd_register_key(&sys->kbd, C64_KEY_F8      , 3, 0, 1);    // F8
}

uint32_t c64_exec(c64_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    uint32_t num_ticks = clk_us_to_ticks(C64_FREQUENCY, micro_seconds);
    uint64_t pins = sys->pins;
    if (0 == sys->debug.callback.func) {
        // run without debug callback
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
// #ifdef __IEC_DEBUG
            world_tick();
// #endif
            pins = _c64_tick(sys, pins);
        }
    }
    else {
        // run with debug callback
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
// #ifdef __IEC_DEBUG
            world_tick();
// #endif
            pins = _c64_tick(sys, pins);
            sys->debug.callback.func(sys->debug.callback.user_data, pins);
        }
    }
    sys->pins = pins;
    kbd_update(&sys->kbd, micro_seconds);
    return num_ticks;
}

void c64_key_down(c64_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->joystick_type == C64_JOYSTICKTYPE_NONE) {
        kbd_key_down(&sys->kbd, key_code);
    }
    else {
        uint8_t m = 0;
        switch (key_code) {
            case 0x20: m = C64_JOYSTICK_BTN; break;
            case 0x08: m = C64_JOYSTICK_LEFT; break;
            case 0x09: m = C64_JOYSTICK_RIGHT; break;
            case 0x0A: m = C64_JOYSTICK_DOWN; break;
            case 0x0B: m = C64_JOYSTICK_UP; break;
            default: kbd_key_down(&sys->kbd, key_code); break;
        }
        if (m != 0) {
            switch (sys->joystick_type) {
                case C64_JOYSTICKTYPE_DIGITAL_1:
                    sys->kbd_joy1_mask |= m;
                    break;
                case C64_JOYSTICKTYPE_DIGITAL_2:
                    sys->kbd_joy2_mask |= m;
                    break;
                case C64_JOYSTICKTYPE_DIGITAL_12:
                    sys->kbd_joy1_mask |= m;
                    sys->kbd_joy2_mask |= m;
                    break;
                default: break;
            }
        }
    }
}

void c64_key_up(c64_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->joystick_type == C64_JOYSTICKTYPE_NONE) {
        kbd_key_up(&sys->kbd, key_code);
    }
    else {
        uint8_t m = 0;
        switch (key_code) {
            case 0x20: m = C64_JOYSTICK_BTN; break;
            case 0x08: m = C64_JOYSTICK_LEFT; break;
            case 0x09: m = C64_JOYSTICK_RIGHT; break;
            case 0x0A: m = C64_JOYSTICK_DOWN; break;
            case 0x0B: m = C64_JOYSTICK_UP; break;
            default: kbd_key_up(&sys->kbd, key_code); break;
        }
        if (m != 0) {
            switch (sys->joystick_type) {
                case C64_JOYSTICKTYPE_DIGITAL_1:
                    sys->kbd_joy1_mask &= ~m;
                    break;
                case C64_JOYSTICKTYPE_DIGITAL_2:
                    sys->kbd_joy2_mask &= ~m;
                    break;
                case C64_JOYSTICKTYPE_DIGITAL_12:
                    sys->kbd_joy1_mask &= ~m;
                    sys->kbd_joy2_mask &= ~m;
                    break;
                default: break;
            }
        }
    }
}

void c64_set_joystick_type(c64_t* sys, c64_joystick_type_t type) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->joystick_type = type;
}

c64_joystick_type_t c64_joystick_type(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->joystick_type;
}

void c64_joystick(c64_t* sys, uint8_t joy1_mask, uint8_t joy2_mask) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->joy_joy1_mask = joy1_mask;
    sys->joy_joy2_mask = joy2_mask;
}

bool c64_quickload(c64_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && data.ptr);
    if (data.size < 2) {
        return false;
    }
    const uint8_t* ptr = (uint8_t*)data.ptr;
    const uint16_t start_addr = ptr[1]<<8 | ptr[0];
    ptr += 2;
    const uint16_t end_addr = start_addr + (data.size - 2);
    uint16_t addr = start_addr;
    while (addr < end_addr) {
        mem_wr(&sys->mem_cpu, addr++, *ptr++);
    }

    // update the BASIC pointers
    mem_wr16(&sys->mem_cpu, 0x2d, end_addr);
    mem_wr16(&sys->mem_cpu, 0x2f, end_addr);
    mem_wr16(&sys->mem_cpu, 0x31, end_addr);
    mem_wr16(&sys->mem_cpu, 0x33, end_addr);
    mem_wr16(&sys->mem_cpu, 0xae, end_addr);

    return true;
}

bool c64_insert_tape(c64_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    return c1530_insert_tape(&sys->c1530, data);
}

void c64_remove_tape(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    c1530_remove_tape(&sys->c1530);
}

bool c64_tape_inserted(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    return c1530_tape_inserted(&sys->c1530);
}

void c64_tape_play(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    c1530_play(&sys->c1530);
}

void c64_tape_stop(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    c1530_stop(&sys->c1530);
}

bool c64_is_tape_motor_on(c64_t* sys) {
    CHIPS_ASSERT(sys && sys->valid && sys->c1530.valid);
    return c1530_is_motor_on(&sys->c1530);
}

chips_display_info_t c64_display_info(c64_t* sys) {
    chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = M6569_FRAMEBUFFER_WIDTH,
                .height = M6569_FRAMEBUFFER_HEIGHT,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = M6569_FRAMEBUFFER_SIZE_BYTES,
            }
        },
        .palette = m6569_dbg_palette(),
    };
    if (sys) {
        res.screen = m6569_screen(&sys->vic);
    }
    else {
        res.screen = (chips_rect_t){
            .x = 0,
            .y = 0,
            .width = _C64_SCREEN_WIDTH,
            .height = _C64_SCREEN_HEIGHT
        };
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    return res;
}

uint32_t c64_save_snapshot(c64_t* sys, c64_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio.callback);
    m6502_snapshot_onsave(&dst->cpu);
    m6569_snapshot_onsave(&dst->vic);
    mem_snapshot_onsave(&dst->mem_cpu, sys);
    mem_snapshot_onsave(&dst->mem_vic, sys);
    c1530_snapshot_onsave(&dst->c1530);
    c1541_snapshot_onsave(&dst->c1541, sys);
    return C64_SNAPSHOT_VERSION;
}

bool c64_load_snapshot(c64_t* sys, uint32_t version, c64_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != C64_SNAPSHOT_VERSION) {
        return false;
    }
    static c64_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    chips_audio_callback_snapshot_onload(&im.audio.callback, &sys->audio.callback);
    m6502_snapshot_onload(&im.cpu, &sys->cpu);
    m6569_snapshot_onload(&im.vic, &sys->vic);
    mem_snapshot_onload(&im.mem_cpu, sys);
    mem_snapshot_onload(&im.mem_vic, sys);
    c1530_snapshot_onload(&im.c1530, &sys->c1530);
    c1541_snapshot_onload(&im.c1541, &sys->c1541, sys);
    *sys = im;
    return true;
}

void c64_basic_run(c64_t* sys) {
    CHIPS_ASSERT(sys);
    // write RUN into the keyboard buffer
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'R');
    mem_wr(&sys->mem_cpu, keybuf++, 'U');
    mem_wr(&sys->mem_cpu, keybuf++, 'N');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 4);
}

void c64_basic_load(c64_t* sys) {
    CHIPS_ASSERT(sys);
    // write LOAD
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'L');
    mem_wr(&sys->mem_cpu, keybuf++, 'O');
    mem_wr(&sys->mem_cpu, keybuf++, 'A');
    mem_wr(&sys->mem_cpu, keybuf++, 'D');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 5);
}

void c64_basic_syscall(c64_t* sys, uint16_t addr) {
    CHIPS_ASSERT(sys);
    // write SYS xxxx[Return] into the keyboard buffer (up to 10 chars)
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'S');
    mem_wr(&sys->mem_cpu, keybuf++, 'Y');
    mem_wr(&sys->mem_cpu, keybuf++, 'S');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 10000) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 1000) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 100) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 10) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 1) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 9);
}

uint16_t c64_syscall_return_addr(void) {
    return 0xA7EA;
}
#endif /* CHIPS_IMPL */
