#pragma once
/*#
    # w65c816.h

    WDC W65C816 CPU emulator.

    Project repo: https://github.com/floooh/chips/

    NOTE: this file is hand-written (m6502.h is code-generated from the
    codegen directory, this may or may not happen for the 65816 too).

    Implementation status: complete. All 256 opcodes pass the full
    SingleStepTests_65816 suite (5.12 million cycle-exact tests in
    native and emulation mode). The interrupt pins (IRQ/NMI/ABORT/RES),
    RDY and the WAI/STP halt states are implemented following the WDC
    datasheet, they are not covered by the test suite.

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

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *   IRQ --->|           |---> A0  *
    *   NMI --->|           |...      *
    *  ABORT--->|           |---> A23 *
    *   RES--->|            |         *
    *   RDY--->|            |         *
    *    RW <---|           |         *
    *   VPA <---|           |         *
    *   VDA <---|           |<--> D0  *
    *   VPB <---|           |...      *
    *   MLB <---|           |<--> D7  *
    *  SYNC <---|           |         *
    *  (E)  <---|           |         *
    *  (MX) <---|           |         *
    *           +-----------+         *
    ***********************************

    ## Overview

    w65c816.h implements a cycle-stepped W65C816 CPU emulator, meaning
    that the emulation state can be ticked forward in clock cycles instead
    of full instructions, and all memory accesses are performed through
    a 64-bit pin mask, just like the other chips CPU emulators.

    The 65816 has a 24-bit address bus (A0..A23 in pin bits 0..23), an
    8-bit data bus (D0..D7 in pin bits 24..31), and the following
    control pins:

        RW      out: 1 for memory read, 0 for memory write
        VPA     out: valid program address (opcode/operand fetch)
        VDA     out: valid data address (data memory access)
        VPB     out: valid program bank address (vector fetch)
        MLB     out: memory lock (asserted during read-modify-write cycles)
        E       out: current emulation mode flag state
        MXM     out: current M status flag state (8-bit A/memory)
        MXX     out: current X status flag state (8-bit X/Y)
        SYNC    out: set on the opcode fetch cycle (chips-specific helper
                     pin, the real 65816 only has VPA/VDA/VPB)

    Machine state is transferred through pins: set the RES pin to start
    a reset sequence, the IRQ or NMI pins to request an interrupt.

    A minimal execution loop looks like this:

    ~~~C
    // 16 MBytes of RAM
    uint8_t mem[1<<24];
    w65c816_t cpu;
    uint64_t pins = w65c816_init(&cpu, &(w65c816_desc_t){0});
    while (...) {
        pins = w65c816_tick(&cpu, pins);
        const uint32_t addr = W65C816_GET_ADDR(pins);
        if (pins & W65C816_RW) {
            W65C816_SET_DATA(pins, mem[addr]);
        }
        else {
            mem[addr] = W65C816_GET_DATA(pins);
        }
    }
    ~~~

    NOTE: the 24-bit address bus means that the memory access code must
    handle bank windowing (usually through a pagetable).

    ## Functions
    ~~~C
    uint64_t w65c816_init(w65c816_t* cpu, const w65c816_desc_t* desc)
    uint64_t w65c816_tick(w65c816_t* cpu, uint64_t pins)
    // plus register get/set functions (see below)
    ~~~

    ## zlib/libpng license

    Copyright (c) 2026 Andre Weissflog
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

#ifdef __cplusplus
extern "C" {
#endif

// address bus pins (24-bit address space)
#define W65C816_PIN_A0    (0)
#define W65C816_PIN_A1    (1)
#define W65C816_PIN_A2    (2)
#define W65C816_PIN_A3    (3)
#define W65C816_PIN_A4    (4)
#define W65C816_PIN_A5    (5)
#define W65C816_PIN_A6    (6)
#define W65C816_PIN_A7    (7)
#define W65C816_PIN_A8    (8)
#define W65C816_PIN_A9    (9)
#define W65C816_PIN_A10   (10)
#define W65C816_PIN_A11   (11)
#define W65C816_PIN_A12   (12)
#define W65C816_PIN_A13   (13)
#define W65C816_PIN_A14   (14)
#define W65C816_PIN_A15   (15)
#define W65C816_PIN_A16   (16)
#define W65C816_PIN_A17   (17)
#define W65C816_PIN_A18   (18)
#define W65C816_PIN_A19   (19)
#define W65C816_PIN_A20   (20)
#define W65C816_PIN_A21   (21)
#define W65C816_PIN_A22   (22)
#define W65C816_PIN_A23   (23)

// data bus pins
#define W65C816_PIN_D0    (24)
#define W65C816_PIN_D1    (25)
#define W65C816_PIN_D2    (26)
#define W65C816_PIN_D3    (27)
#define W65C816_PIN_D4    (28)
#define W65C816_PIN_D5    (29)
#define W65C816_PIN_D6    (30)
#define W65C816_PIN_D7    (31)

// control pins
#define W65C816_PIN_RW    (32)     // out: 1=read, 0=write
#define W65C816_PIN_VPA   (33)     // out: valid program address
#define W65C816_PIN_VDA   (34)     // out: valid data address
#define W65C816_PIN_VPB   (35)     // out: valid program bank address (vector fetch)
#define W65C816_PIN_MLB   (36)     // out: memory lock (read-modify-write)
#define W65C816_PIN_E     (37)     // out: emulation mode flag
#define W65C816_PIN_MXM   (38)     // out: M flag state (8-bit accumulator)
#define W65C816_PIN_MXX   (39)     // out: X flag state (8-bit index regs)
#define W65C816_PIN_RDY   (40)     // in: freeze execution at next read cycle
#define W65C816_PIN_SYNC  (41)     // out: opcode fetch cycle (chips-specific)
#define W65C816_PIN_IRQ   (42)     // in: maskable interrupt requested
#define W65C816_PIN_NMI   (43)     // in: non-maskable interrupt requested
#define W65C816_PIN_ABORT (44)     // in: abort requested
#define W65C816_PIN_RES   (45)     // in: request RESET

// pin bit masks
#define W65C816_A0    (1ULL<<W65C816_PIN_A0)
#define W65C816_A8    (1ULL<<W65C816_PIN_A8)
#define W65C816_A16   (1ULL<<W65C816_PIN_A16)
#define W65C816_D0    (1ULL<<W65C816_PIN_D0)
#define W65C816_RW    (1ULL<<W65C816_PIN_RW)
#define W65C816_VPA   (1ULL<<W65C816_PIN_VPA)
#define W65C816_VDA   (1ULL<<W65C816_PIN_VDA)
#define W65C816_VPB   (1ULL<<W65C816_PIN_VPB)
#define W65C816_MLB   (1ULL<<W65C816_PIN_MLB)
#define W65C816_E     (1ULL<<W65C816_PIN_E)
#define W65C816_MXM   (1ULL<<W65C816_PIN_MXM)
#define W65C816_MXX   (1ULL<<W65C816_PIN_MXX)
#define W65C816_RDY   (1ULL<<W65C816_PIN_RDY)
#define W65C816_SYNC  (1ULL<<W65C816_PIN_SYNC)
#define W65C816_IRQ   (1ULL<<W65C816_PIN_IRQ)
#define W65C816_NMI   (1ULL<<W65C816_PIN_NMI)
#define W65C816_ABORT (1ULL<<W65C816_PIN_ABORT)
#define W65C816_RES   (1ULL<<W65C816_PIN_RES)

/* bit mask for all CPU pins (up to bit pos 46) */
#define W65C816_PIN_MASK ((1ULL<<46)-1)

/* status flag bits in P (WDC 65816 layout) */
#define W65C816_CF    (1<<0)  /* carry */
#define W65C816_ZF    (1<<1)  /* zero */
#define W65C816_IF    (1<<2)  /* IRQ disable */
#define W65C816_DF    (1<<3)  /* decimal mode */
#define W65C816_XF    (1<<4)  /* index select: 1=8-bit X/Y (doubles as B flag) */
#define W65C816_MF    (1<<5)  /* memory/accumulator select: 1=8-bit */
#define W65C816_VF    (1<<6)  /* overflow */
#define W65C816_NF    (1<<7)  /* negative */

/* internal BRK state flags */
#define W65C816_BRK_IRQ   (1<<0)  /* IRQ was triggered */
#define W65C816_BRK_NMI   (1<<1)  /* NMI was triggered */
#define W65C816_BRK_RESET (1<<2)  /* RES was triggered */
#define W65C816_BRK_ABORT (1<<3)  /* ABORT was triggered */

/* the desc structure provided to w65c816_init() */
typedef struct {
    bool abort_disabled;    /* ignore the ABORT pin */
} w65c816_desc_t;

/* CPU state */
typedef struct {
    uint16_t IR;        /* internal instruction register: (opcode<<4)|microstep */
    uint16_t PC;        /* program counter (relative to PBR) */
    uint16_t C;         /* 16-bit accumulator (A=low byte, B=high byte) */
    uint16_t X, Y;      /* index registers */
    uint16_t S;         /* stack pointer (in emulation mode: 0x01xx page) */
    uint16_t D;         /* direct page register */
    uint8_t PBR;        /* program bank register */
    uint8_t DBR;        /* data bank register */
    uint8_t P;          /* status flags */
    uint8_t E;          /* emulation mode flag */
    /* internal state */
    uint32_t AD;        /* 24-bit effective address latch */
    uint16_t TA;        /* temp latch (address bytes / dp offset) */
    uint16_t TD;        /* data latch (8 or 16-bit operand data) */
    uint16_t AA;        /* temp address (e.g. pointer location) */
    uint16_t RR;        /* temp result latch (read-modify-write) */
    uint16_t TB;        /* temp bank/pointer latch (16-bit: lo|mid<<8) */
    uint8_t CRS;        /* page-crossing flag */
    uint16_t MC;        /* block move counter (MVN/MVP) */
    uint8_t halted;     /* 0: running, 1: WAI, 2: STP */
    uint64_t PINS;      /* last stored pin state (do NOT modify) */
    uint16_t irq_pip;
    uint16_t nmi_pip;
    uint16_t abrt_pip;
    uint8_t brk_flags;  /* W65C816_BRK_* */
} w65c816_t;

/* initialize a new w65c816 instance and return initial pin mask */
uint64_t w65c816_init(w65c816_t* cpu, const w65c816_desc_t* desc);
/* execute one tick */
uint64_t w65c816_tick(w65c816_t* cpu, uint64_t pins);
/* prepare w65c816_t snapshot for saving */
void w65c816_snapshot_onsave(w65c816_t* snapshot);
/* fixup w65c816_t snapshot after loading */
void w65c816_snapshot_onload(w65c816_t* snapshot, w65c816_t* sys);

/* register access functions */
void w65c816_set_a(w65c816_t* cpu, uint8_t v);      /* low byte of C */
void w65c816_set_c(w65c816_t* cpu, uint16_t v);     /* full 16-bit accumulator */
void w65c816_set_x(w65c816_t* cpu, uint16_t v);
void w65c816_set_y(w65c816_t* cpu, uint16_t v);
void w65c816_set_s(w65c816_t* cpu, uint16_t v);
void w65c816_set_d(w65c816_t* cpu, uint16_t v);
void w65c816_set_p(w65c816_t* cpu, uint8_t v);
void w65c816_set_pbr(w65c816_t* cpu, uint8_t v);
void w65c816_set_dbr(w65c816_t* cpu, uint8_t v);
void w65c816_set_pc(w65c816_t* cpu, uint16_t v);
void w65c816_set_e(w65c816_t* cpu, bool v);
uint8_t w65c816_a(w65c816_t* cpu);
uint16_t w65c816_c(w65c816_t* cpu);
uint16_t w65c816_x(w65c816_t* cpu);
uint16_t w65c816_y(w65c816_t* cpu);
uint16_t w65c816_s(w65c816_t* cpu);
uint16_t w65c816_d(w65c816_t* cpu);
uint8_t w65c816_p(w65c816_t* cpu);
uint8_t w65c816_pbr(w65c816_t* cpu);
uint8_t w65c816_dbr(w65c816_t* cpu);
uint16_t w65c816_pc(w65c816_t* cpu);
bool w65c816_e(w65c816_t* cpu);

/* extract 24-bit address bus from 64-bit pins */
#define W65C816_GET_ADDR(p) ((uint32_t)((p)&0xFFFFFFULL))
/* merge 24-bit address bus value into 64-bit pins */
#define W65C816_SET_ADDR(p,a) {p=(((p)&~0xFFFFFFULL)|((uint32_t)(a)&0xFFFFFFULL));}
/* extract 8-bit data bus from 64-bit pins */
#define W65C816_GET_DATA(p) ((uint8_t)(((p)&0xFF000000ULL)>>24))
/* merge 8-bit data bus value into 64-bit pins */
#define W65C816_SET_DATA(p,d) {p=(((p)&~0xFF000000ULL)|((((uint32_t)(d))<<24)&0xFF000000ULL));}
/* copy data bus value from other pin mask */
#define W65C816_COPY_DATA(p0,p1) (((p0)&~0xFF000000ULL)|((p1)&0xFF000000ULL))
/* return a pin mask with control-pins, address and data bus */
#define W65C816_MAKE_PINS(ctrl, addr, data) ((ctrl)|((((uint64_t)(data))<<24)&0xFF000000ULL)|((uint32_t)(addr)&0xFFFFFFULL))

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* register access functions */
void w65c816_set_a(w65c816_t* cpu, uint8_t v) { cpu->C = (cpu->C & 0xFF00) | v; }
void w65c816_set_c(w65c816_t* cpu, uint16_t v) { cpu->C = v; }
void w65c816_set_x(w65c816_t* cpu, uint16_t v) { cpu->X = v; }
void w65c816_set_y(w65c816_t* cpu, uint16_t v) { cpu->Y = v; }
void w65c816_set_s(w65c816_t* cpu, uint16_t v) { cpu->S = v; }
void w65c816_set_d(w65c816_t* cpu, uint16_t v) { cpu->D = v; }
void w65c816_set_p(w65c816_t* cpu, uint8_t v) { cpu->P = v; }
void w65c816_set_pbr(w65c816_t* cpu, uint8_t v) { cpu->PBR = v; }
void w65c816_set_dbr(w65c816_t* cpu, uint8_t v) { cpu->DBR = v; }
void w65c816_set_pc(w65c816_t* cpu, uint16_t v) { cpu->PC = v; }
void w65c816_set_e(w65c816_t* cpu, bool v) { cpu->E = v; }
uint8_t w65c816_a(w65c816_t* cpu) { return (uint8_t)cpu->C; }
uint16_t w65c816_c(w65c816_t* cpu) { return cpu->C; }
uint16_t w65c816_x(w65c816_t* cpu) { return cpu->X; }
uint16_t w65c816_y(w65c816_t* cpu) { return cpu->Y; }
uint16_t w65c816_s(w65c816_t* cpu) { return cpu->S; }
uint16_t w65c816_d(w65c816_t* cpu) { return cpu->D; }
uint8_t w65c816_p(w65c816_t* cpu) { return cpu->P; }
uint8_t w65c816_pbr(w65c816_t* cpu) { return cpu->PBR; }
uint8_t w65c816_dbr(w65c816_t* cpu) { return cpu->DBR; }
uint16_t w65c816_pc(w65c816_t* cpu) { return cpu->PC; }
bool w65c816_e(w65c816_t* cpu) { return 0 != cpu->E; }

/* helper macros for the code-generated instruction decoder */
/* set 24-bit address in 64-bit pin mask */
#define _SA(addr) pins=(pins&~0xFFFFFFULL)|((uint32_t)(addr)&0xFFFFFFULL)
/* set 24-bit address and 8-bit data in 64-bit pin mask (keeps control pins) */
#define _SAD(addr,data) pins=(pins&~0xFFFFFFFFULL)|((uint32_t)(addr)&0xFFFFFFULL)|((((uint32_t)(data))<<24)&0xFF000000ULL)
/* set 8-bit data in 64-bit pin mask */
#define _SD(data) pins=(pins&~0xFF000000ULL)|((((uint32_t)(data))<<24)&0xFF000000ULL)
/* extract 8-bit data from 64-bit pin mask */
#define _GD() ((uint8_t)((pins&0xFF000000ULL)>>24))
/* set address to PBR:PC (opcode fetch position) */
#define _PB_PC() ((((uint32_t)c->PBR)<<16)|c->PC)
/* enable control pins */
#define _ON(m) pins|=(m)
/* disable control pins */
#define _OFF(m) pins&=~(m)
/* a memory read tick (reads are default, this is just for readability) */
#define _RD() _ON(W65C816_RW)
/* a memory write tick */
#define _WR() _OFF(W65C816_RW)
/* valid-program-address cycle */
#define _VPA() _ON(W65C816_VPA)
/* valid-data-address cycle */
#define _VDA() _ON(W65C816_VDA)
/* valid-program-bank-address cycle (vector fetch) */
#define _VPB() _ON(W65C816_VPB)
/* memory-lock cycle (read-modify-write) */
#define _MLB() _ON(W65C816_MLB)
/* fetch next opcode byte (the next instruction's first bus cycle) */
#define _FETCH() _SA(_PB_PC());_ON(W65C816_SYNC);_VPA();_VDA()
/* internal cycle: dummy read at PB:PC without any valid-address pins */
#define _DUMMY() _SA(_PB_PC())
/* dummy read at the address of the previously fetched operand byte */
#define _DUMMY_PP() _SA((((uint32_t)c->PBR)<<16)|(uint16_t)(c->PC-1))
/* set N and Z flags depending on 8-bit value */
#define _NZ8(v) c->P=((c->P&~(W65C816_NF|W65C816_ZF))|((v&0xFF)?(v&W65C816_NF):W65C816_ZF))
/* set N and Z flags depending on 16-bit value */
#define _NZ16(v) c->P=((c->P&~(W65C816_NF|W65C816_ZF))|((v&0xFFFF)?(((v)>>8)&W65C816_NF):W65C816_ZF))
/* true if accumulator/memory operations are 8-bit */
#define _W8() (c->E||(c->P&W65C816_MF))
/* true if index register operations are 8-bit */
#define _X8() (c->E||(c->P&W65C816_XF))
/* effective stack pointer (in emulation mode, S is forced to page one) */
#define _SH() (c->E?(uint16_t)(0x0100|(c->S&0xFF)):c->S)
/* stack pointer +/- n, wrapping inside page 1 in emulation mode */
#define _SPADD(n) (c->E?(uint16_t)(0x0100|((c->S+(n))&0xFF)):(uint16_t)(c->S+(n)))
/* like _SPADD, but the address is computed linearly (no page-1 wrap) from
   the forced emulation-mode stack pointer (used by PLB/PLD/RTL) */
#define _SPLIN(n) (c->E?(uint16_t)((0x0100|(c->S&0xFF))+(n)):(uint16_t)(c->S+(n)))
/* output the emulation mode and M/X flag state pins */
#define _STATUS_PINS() {if(c->E){_ON(W65C816_E);}else{_OFF(W65C816_E);}if(c->P&W65C816_MF){_ON(W65C816_MXM);}else{_OFF(W65C816_MXM);}if(c->P&W65C816_XF){_ON(W65C816_MXX);}else{_OFF(W65C816_MXX);}}

/*--- ALU operation macros (width-aware, applied to the latched data) ---*/
#define _ORA(v) do { if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|((c->C|(v))&0xFF)); _NZ8(c->C); } else { c->C=(uint16_t)(c->C|(v)); _NZ16(c->C); } } while (0)
#define _AND(v) do { if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|((c->C&(v))&0xFF)); _NZ8(c->C); } else { c->C=(uint16_t)(c->C&(v)); _NZ16(c->C); } } while (0)
#define _EOR(v) do { if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|((c->C^(v))&0xFF)); _NZ8(c->C); } else { c->C=(uint16_t)(c->C^(v)); _NZ16(c->C); } } while (0)
#define _ADC(v) do { _w65c816_adc(c,(uint16_t)(v),!_W8()); } while (0)
#define _SBC(v) do { _w65c816_sbc(c,(uint16_t)(v),!_W8()); } while (0)
#define _LDA(v) do { if (_W8()) { c->C=(uint16_t)(((v)&0xFF)|(c->C&0xFF00)); _NZ8(c->C); } else { c->C=(v); _NZ16(c->C); } } while (0)
#define _CMPA(v) do { if (_W8()) { uint32_t t=(uint32_t)(c->C&0xFF)-((v)&0xFF); _NZ8((uint16_t)t); if (0==(t&0x100)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } else { uint32_t t=(uint32_t)c->C-((v)&0xFFFF); _NZ16((uint16_t)t); if (0==(t&0x10000)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } } while (0)

/*--- addressing mode microcode macros (read flavour) ---
   slot layout notes:
   - slot 0 is executed in the same tick that loads the opcode from the
     instruction-fetch cycle, it produces bus cycle 2 of the instruction
   - conditional extra cycles (D-alignment penalty, page crossing) are
     decided in the slot *before* the extra slot by advancing c->IR
   - the last addressing slot branches by width: 8-bit ops execute and
     fetch the next opcode, 16-bit ops read/write the high byte first
---*/

/* shared 2-slot tail for all read modes: consume the low data byte and
   either execute (8-bit) or read the high byte and execute (16-bit)
   ...'nextaddr' is the address expression for the high data byte
*/
#define _TAIL_RD(op, s, exec, nextaddr) \
    case ((op)<<4)|(s): c->TD=_GD(); if (_W8()) { exec(c->TD); _FETCH(); } else { _SA(nextaddr); _VDA(); } break; \
    case ((op)<<4)|((s)+1): c->TD|=((uint16_t)_GD())<<8; exec(c->TD); _FETCH(); break;

/* immediate: 2 cycles (8-bit) / 3 cycles (16-bit) */
#define _M_IMM_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (_W8()) { c->IR++; } break; \
    case ((op)<<4)|1: _SA(_PB_PC()); c->PC++; _VPA(); c->TA=_GD(); break; \
    case ((op)<<4)|2: if (_W8()) { exec(_GD()); } else { exec((uint16_t)(c->TA|(((uint16_t)_GD())<<8))); } _FETCH(); break;

/* direct page: 3 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DP_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 3, exec, ((c->AD+1)&0xFFFF))

/* direct page indexed (X or Y): 4 cycles + 1 if D low byte != 0 + 1 if 16-bit.
   QUIRK (verified against SingleStepTests): with 8-bit index registers and a
   page-aligned direct page, the index add wraps at 8 bits (the carry into
   the high byte is discarded); in all other cases the index add is a full
   16-bit add.
*/
#define _M_DPXY_RD(op, exec, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->TA=(uint16_t)(c->D+(uint8_t)(dpb+(idx))); } \
        else { c->TA=(uint16_t)(dpb+c->D+(idx)); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA(c->TA); _VDA(); break; \
    _TAIL_RD(op, 4, exec, ((c->TA+1)&0xFFFF))

/* absolute: 4 cycles + 1 if 16-bit */
#define _M_ABS_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AD=((((uint32_t)c->DBR)<<16)|(((uint16_t)_GD())<<8)|c->TA); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 3, exec, ((c->AD+1)&0xFFFFFF))

/* absolute indexed (X or Y): 4 cycles + 1 fixup + 1 if 16-bit data.
   - 8-bit index (X flag set or emulation mode): fixup cycle only on page cross
   - 16-bit index: fixup cycle always (the low-byte add can't merge)
   NOTE: the carry out of the 16-bit index add propagates into the bank byte!
*/
#define _M_ABXY_RD(op, exec, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+(uint16_t)(idx); \
        c->CRS=((base^sum32)&0xFF00)?1:0; c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); \
        if (_X8() && !c->CRS) { _VDA(); c->IR++; } } break; \
    case ((op)<<4)|3: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); break; \
    _TAIL_RD(op, 4, exec, ((c->AD+1)&0xFFFFFF))

/* (dp,X): 6 cycles + 1 if D low byte != 0 + 1 if 16-bit
   (same 8-bit-index wrap quirk on page-aligned D as dp,X)
*/
#define _M_IDX_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->AA=(uint16_t)(c->D+(uint8_t)(dpb+c->X)); } \
        else { c->AA=(uint16_t)(dpb+c->D+c->X); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->TA|=((uint16_t)_GD())<<8; c->AD=((((uint32_t)c->DBR)<<16)|c->TA); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 6, exec, (((c->AD&0xFF0000u)|((c->AD+1)&0xFFFF))))

/* (dp): 5 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DPIND_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; c->AD=((((uint32_t)c->DBR)<<16)|c->TA); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 5, exec, (((c->AD&0xFF0000u)|((c->AD+1)&0xFFFF))))

/* (dp),Y: 5 cycles + 1 if D low byte != 0 + 1 on page cross + 1 if 16-bit
    NOTE: like abs,X/abs,Y the 16-bit add carry propagates into the bank byte.
    Fixup rule as in abs,X/abs,Y: unconditional with 16-bit index, otherwise
    only on page cross.
*/
#define _M_IDY_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+c->Y; \
        c->CRS=((base^sum32)&0xFF00)?1:0; c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); \
        if (_X8() && !c->CRS) { _VDA(); c->IR++; } } break; \
    case ((op)<<4)|5: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); break; \
    _TAIL_RD(op, 6, exec, ((c->AD+1)&0xFFFFFF))

/* [dp] long indirect: 6 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_IDL_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TB=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TB|=((uint16_t)_GD())<<8; _SA((c->AA+2)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->AD=((((uint32_t)_GD())<<16)|c->TB); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 6, exec, ((c->AD+1)&0xFFFFFF))

/* [dp],Y long indirect indexed: same as [dp], Y added to the 24-bit pointer
   (no page-cross penalty), 6 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_IDLY_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TB=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TB|=((uint16_t)_GD())<<8; _SA((c->AA+2)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->AD=(((((uint32_t)_GD())<<16)|c->TB)+c->Y)&0xFFFFFF; _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 6, exec, ((c->AD+1)&0xFFFFFF))

/* stack relative: 4 cycles + 1 if 16-bit */
#define _M_SR_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)(_SH()+c->TA); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 3, exec, ((c->AD+1)&0xFFFF))

/* (sr,S),Y: 7 cycles + 1 if 16-bit (no page-cross penalty) */
#define _M_SRIY_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)(_SH()+c->TA); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; _SA((c->AA+1)&0xFFFF); break; \
    case ((op)<<4)|5: { uint32_t sum32=(uint32_t)c->TA+c->Y; \
        c->AD=((((uint32_t)((c->DBR+(sum32>>16))&0xFF))<<16)|(sum32&0xFFFF)); \
        _SA(c->AD); _VDA(); } break; \
    _TAIL_RD(op, 6, exec, ((c->AD+1)&0xFFFFFF))

/* absolute long: 5 cycles + 1 if 16-bit */
#define _M_ABL_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->AD=((((uint32_t)_GD())<<16)|c->TB); _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 4, exec, ((c->AD+1)&0xFFFFFF))

/* absolute long indexed X: same as al, X added to the 24-bit base address */
#define _M_ABLX_RD(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->AD=(((((uint32_t)_GD())<<16)|c->TB)+c->X)&0xFFFFFF; _SA(c->AD); _VDA(); break; \
    _TAIL_RD(op, 4, exec, ((c->AD+1)&0xFFFFFF))

/* X-flag-width read tail for the index register ops (LDX/LDY/CPX/CPY):
   the data width follows the X flag, not M */
#define _TAIL_RDX(op, s, exec, nextaddr) \
    case ((op)<<4)|(s): c->TD=_GD(); if (_X8()) { exec(c->TD); _FETCH(); } else { _SA(nextaddr); _VDA(); } break; \
    case ((op)<<4)|((s)+1): c->TD|=((uint16_t)_GD())<<8; exec(c->TD); _FETCH(); break;

#define _M_DP_RDX(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AD); _VDA(); break; \
    _TAIL_RDX(op, 3, exec, ((c->AD+1)&0xFFFF))

#define _M_ABS_RDX(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AD=((((uint32_t)c->DBR)<<16)|(((uint16_t)_GD())<<8)|c->TA); _SA(c->AD); _VDA(); break; \
    _TAIL_RDX(op, 3, exec, ((c->AD+1)&0xFFFFFF))

#define _M_DPXY_RDX(op, exec, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->TA=(uint16_t)(c->D+(uint8_t)(dpb+(idx))); } \
        else { c->TA=(uint16_t)(dpb+c->D+(idx)); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA(c->TA); _VDA(); break; \
    _TAIL_RDX(op, 4, exec, ((c->TA+1)&0xFFFF))

#define _M_ABXY_RDX(op, exec, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+(uint16_t)(idx); \
        c->CRS=((base^sum32)&0xFF00)?1:0; c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); \
        if (_X8() && !c->CRS) { _VDA(); c->IR++; } } break; \
    case ((op)<<4)|3: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); break; \
    _TAIL_RDX(op, 4, exec, ((c->AD+1)&0xFFFFFF))

/*--- addressing mode microcode macros (write flavour) ---
   write cycles never merge with the effective-address fixup, and writes
   never get a page-cross penalty; 16-bit writes go out low byte first
---*/

/* direct page write: 3 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DP_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|3: _SA((c->AD+1)&0xFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|4: _FETCH(); break;

/* direct page indexed write (X or Y): 4 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DPXY_WR(op, reg, w8, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->TA=(uint16_t)(c->D+(uint8_t)(dpb+(idx))); } \
        else { c->TA=(uint16_t)(dpb+c->D+(idx)); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA(c->TA); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|4: _SA((c->TA+1)&0xFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|5: _FETCH(); break;

/* absolute write: 4 cycles + 1 if 16-bit */
#define _M_ABS_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AD=((((uint32_t)c->DBR)<<16)|(((uint16_t)_GD())<<8)|c->TA); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|3: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|4: _FETCH(); break;

/* absolute indexed write (X or Y): 5 cycles + 1 if 16-bit (fixup cycle is
   unconditional for writes, carry out of the index add goes into the bank)
*/
#define _M_ABXY_WR(op, reg, w8, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+(uint16_t)(idx); \
        c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); } break; \
    case ((op)<<4)|3: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|4: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|5: _FETCH(); break;

/* (dp) write: 5 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DPIND_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; c->AD=((((uint32_t)c->DBR)<<16)|c->TA); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|5: _SA(((c->AD&0xFF0000u)|((c->AD+1)&0xFFFF))); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|6: _FETCH(); break;

/* (dp,X) write: 6 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_IDX_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->AA=(uint16_t)(c->D+(uint8_t)(dpb+c->X)); } \
        else { c->AA=(uint16_t)(dpb+c->D+c->X); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->TA|=((uint16_t)_GD())<<8; c->AD=((((uint32_t)c->DBR)<<16)|c->TA); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|6: _SA(((c->AD&0xFF0000u)|((c->AD+1)&0xFFFF))); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* (dp),Y write: 6 cycles + 1 if D low byte != 0 + 1 if 16-bit
   (the Y-add is a separate dummy cycle, no page-cross penalty, the 16-bit
   index add carry propagates into the bank byte)
*/
#define _M_IDY_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+c->Y; \
        c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); } break; \
    case ((op)<<4)|5: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|6: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* [dp] long indirect write: 6 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_IDL_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TB=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TB|=((uint16_t)_GD())<<8; _SA((c->AA+2)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->AD=((((uint32_t)_GD())<<16)|c->TB); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|6: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* [dp],Y long indirect indexed write: 6 cycles + 1 if D low byte != 0 + 1 if
   16-bit (Y is added inline to the 24-bit pointer, no page-cross penalty) */
#define _M_IDLY_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TB=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TB|=((uint16_t)_GD())<<8; _SA((c->AA+2)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->AD=(((((uint32_t)_GD())<<16)|c->TB)+c->Y)&0xFFFFFF; _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|6: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* stack relative write: 4 cycles + 1 if 16-bit */
#define _M_SR_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)(_SH()+c->TA); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|3: _SA((c->AD+1)&0xFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|4: _FETCH(); break;

/* (sr,S),Y write: 7 cycles + 1 if 16-bit */
#define _M_SRIY_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)(_SH()+c->TA); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; _SA((c->AA+1)&0xFFFF); break; \
    case ((op)<<4)|5: { uint32_t sum32=(uint32_t)c->TA+c->Y; \
        c->AD=((((uint32_t)((c->DBR+(sum32>>16))&0xFF))<<16)|(sum32&0xFFFF)); \
        _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } } break; \
    case ((op)<<4)|6: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* absolute long write: 5 cycles + 1 if 16-bit */
#define _M_ABL_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->AD=((((uint32_t)_GD())<<16)|c->TB); _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|4: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|5: _FETCH(); break;

/* absolute long indexed X write: 5 cycles + 1 if 16-bit */
#define _M_ABLX_WR(op, reg, w8) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->AD=(((((uint32_t)_GD())<<16)|c->TB)+c->X)&0xFFFFFF; _SA(c->AD); _VDA(); _SD((reg)&0xFF); _WR(); if (w8) { c->IR++; } break; \
    case ((op)<<4)|4: _SA((c->AD+1)&0xFFFFFF); _VDA(); _SD((((reg)>>8)&0xFF)); _WR(); break; \
    case ((op)<<4)|5: _FETCH(); break;

/*--- read/modify/write macros (ASL/LSR/ROL/ROR/INC/DEC/TRB/TSB) ---
   the MLB pin is asserted on all read, dummy and write cycles; 16-bit
   RMWs read low/high, dummy at the high address, then write high/low
---*/
#define _RMW_ASL(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|((x&0x80)?W65C816_CF:0)); x=(uint16_t)((x<<1)&0xFF); _NZ8(x); c->RR=x; } else { uint32_t x=(uint32_t)((v)&0xFFFF); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|((x&0x8000)?W65C816_CF:0)); x=(x<<1)&0xFFFF; _NZ16((uint16_t)x); c->RR=(uint16_t)x; } } while (0)
#define _RMW_LSR(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|((x&1)?W65C816_CF:0)); x=(uint16_t)(x>>1); _NZ8(x); c->RR=x; } else { uint32_t x=(uint32_t)((v)&0xFFFF); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|((x&1)?W65C816_CF:0)); x>>=1; _NZ16((uint16_t)x); c->RR=(uint16_t)x; } } while (0)
#define _RMW_ROL(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); const unsigned nc=(x>>7)&1; x=(uint16_t)(((x<<1)|((c->P&W65C816_CF)?1:0))&0xFF); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|(nc?W65C816_CF:0)); _NZ8(x); c->RR=x; } else { uint32_t x=(uint32_t)((v)&0xFFFF); const unsigned nc=(x>>15)&1; x=((x<<1)|((c->P&W65C816_CF)?1:0))&0xFFFF; c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|(nc?W65C816_CF:0)); _NZ16((uint16_t)x); c->RR=(uint16_t)x; } } while (0)
#define _RMW_ROR(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); const unsigned nc=x&1; x=(uint16_t)((x>>1)|((c->P&W65C816_CF)?0x80:0)); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|(nc?W65C816_CF:0)); _NZ8(x); c->RR=x; } else { uint32_t x=(uint32_t)((v)&0xFFFF); const unsigned nc=x&1; x=(x>>1)|((c->P&W65C816_CF)?0x8000:0); c->P=(uint8_t)((c->P&~(W65C816_CF|W65C816_ZF|W65C816_NF))|(nc?W65C816_CF:0)); _NZ16((uint16_t)x); c->RR=(uint16_t)x; } } while (0)
#define _RMW_INC(v) do { if (_W8()) { c->RR=(uint16_t)((((v)&0xFF)+1)&0xFF); _NZ8(c->RR); } else { c->RR=(uint16_t)((((v)&0xFFFF)+1)&0xFFFF); _NZ16(c->RR); } } while (0)
#define _RMW_DEC(v) do { if (_W8()) { c->RR=(uint16_t)((((v)&0xFF)-1)&0xFF); _NZ8(c->RR); } else { c->RR=(uint16_t)((((v)&0xFFFF)-1)&0xFFFF); _NZ16(c->RR); } } while (0)
#define _RMW_TSB(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); c->P=(uint8_t)((c->P&~W65C816_ZF)|(((x&(c->C&0xFF))==0)?W65C816_ZF:0)); c->RR=(uint16_t)((x|(c->C&0xFF))&0xFF); } else { uint32_t x=(uint32_t)((v)&0xFFFF); c->P=(uint8_t)((c->P&~W65C816_ZF)|(((x&c->C)==0)?W65C816_ZF:0)); c->RR=(uint16_t)(x|c->C); } } while (0)
#define _RMW_TRB(v) do { if (_W8()) { uint16_t x=(uint16_t)((v)&0xFF); c->P=(uint8_t)((c->P&~W65C816_ZF)|(((x&(c->C&0xFF))==0)?W65C816_ZF:0)); c->RR=(uint16_t)(x&((~(c->C))&0xFF)); } else { uint32_t x=(uint32_t)((v)&0xFFFF); c->P=(uint8_t)((c->P&~W65C816_ZF)|(((x&c->C)==0)?W65C816_ZF:0)); c->RR=(uint16_t)(x&((~(c->C))&0xFFFF)); } } while (0)

/* direct page RMW: 5 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DP_RMW(op, opr) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AD=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AD); _VDA(); _MLB(); break; \
    case ((op)<<4)|3: c->TD=_GD(); if (c->E) { _SAD(c->AD,(c->TD&0xFF)); _MLB(); _WR(); } else if (_W8()) { _SA(c->AD); _MLB(); } else { _SA((c->AD+1)&0xFFFF); _VDA(); _MLB(); } break; \
    case ((op)<<4)|4: if (_W8()) { opr(c->TD); _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); } else { c->TD|=((uint16_t)_GD())<<8; opr(c->TD); _SA((c->AD+1)&0xFFFF); _MLB(); } break; \
    case ((op)<<4)|5: if (_W8()) { _FETCH(); } else { _SAD((c->AD+1)&0xFFFF,((c->RR>>8)&0xFF)); _VDA(); _MLB(); _WR(); } break; \
    case ((op)<<4)|6: _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* direct page indexed RMW (X): 6 cycles + 1 if D low byte != 0 + 1 if 16-bit */
#define _M_DPXY_RMW(op, opr, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t dpb=(uint8_t)((c->D&0xFF)?c->TA:_GD()); \
        if (c->E && !(c->D&0xFF)) { c->TA=(uint16_t)(c->D+(uint8_t)(dpb+(idx))); } \
        else { c->TA=(uint16_t)(dpb+c->D+(idx)); } } \
        _DUMMY_PP(); break; \
    case ((op)<<4)|3: c->AD=c->TA; _SA(c->AD); _VDA(); _MLB(); break; \
    case ((op)<<4)|4: c->TD=_GD(); if (c->E) { _SAD(c->AD,(c->TD&0xFF)); _MLB(); _WR(); } else if (_W8()) { _SA(c->AD); _MLB(); } else { _SA((c->AD+1)&0xFFFF); _VDA(); _MLB(); } break; \
    case ((op)<<4)|5: if (_W8()) { opr(c->TD); _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); } else { c->TD|=((uint16_t)_GD())<<8; opr(c->TD); _SA((c->AD+1)&0xFFFF); _MLB(); } break; \
    case ((op)<<4)|6: if (_W8()) { _FETCH(); } else { _SAD((c->AD+1)&0xFFFF,((c->RR>>8)&0xFF)); _VDA(); _MLB(); _WR(); } break; \
    case ((op)<<4)|7: _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); break; \
    case ((op)<<4)|8: _FETCH(); break;

/* absolute RMW: 6 cycles + 1 if 16-bit */
#define _M_ABS_RMW(op, opr) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AD=((((uint32_t)c->DBR)<<16)|(((uint16_t)_GD())<<8)|c->TA); _SA(c->AD); _VDA(); _MLB(); break; \
    case ((op)<<4)|3: c->TD=_GD(); if (c->E) { _SAD(c->AD,(c->TD&0xFF)); _MLB(); _WR(); } else if (_W8()) { _SA(c->AD); _MLB(); } else { _SA((c->AD+1)&0xFFFFFF); _VDA(); _MLB(); } break; \
    case ((op)<<4)|4: if (_W8()) { opr(c->TD); _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); } else { c->TD|=((uint16_t)_GD())<<8; opr(c->TD); _SA((c->AD+1)&0xFFFFFF); _MLB(); } break; \
    case ((op)<<4)|5: if (_W8()) { _FETCH(); } else { _SAD((c->AD+1)&0xFFFFFF,((c->RR>>8)&0xFF)); _VDA(); _MLB(); _WR(); } break; \
    case ((op)<<4)|6: _SAD(c->AD,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); break; \
    case ((op)<<4)|7: _FETCH(); break;

/* absolute indexed RMW (X): 7 cycles + 1 if 16-bit (fixup dummy is
   unconditional, the index add carry propagates into the bank byte) */
#define _M_ABXY_RMW(op, opr, idx) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t base=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); \
        uint32_t sum32=(uint32_t)base+(uint16_t)(idx); \
        c->AA=(uint16_t)sum32; \
        c->TB=(uint16_t)((c->DBR+(sum32>>16))&0xFF); \
        c->AD=((((uint32_t)c->TB)<<16)|(sum32&0xFFFF)); \
        _SA(((((uint32_t)c->DBR)<<16)|(base&0xFF00)|(sum32&0xFF))); } break; \
    case ((op)<<4)|3: _SA((((uint32_t)c->TB)<<16)|c->AA); _VDA(); _MLB(); break; \
    case ((op)<<4)|4: c->TD=_GD(); if (c->E) { _SAD((((uint32_t)c->TB)<<16)|c->AA,(c->TD&0xFF)); _MLB(); _WR(); } else if (_W8()) { _SA((((uint32_t)c->TB)<<16)|c->AA); _MLB(); } else { _SA((c->AD+1)&0xFFFFFF); _VDA(); _MLB(); } break; \
    case ((op)<<4)|5: if (_W8()) { opr(c->TD); _SAD((((uint32_t)c->TB)<<16)|c->AA,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); } else { c->TD|=((uint16_t)_GD())<<8; opr(c->TD); _SA((c->AD+1)&0xFFFFFF); _MLB(); } break; \
    case ((op)<<4)|6: if (_W8()) { _FETCH(); } else { _SAD((c->AD+1)&0xFFFFFF,((c->RR>>8)&0xFF)); _VDA(); _MLB(); _WR(); } break; \
    case ((op)<<4)|7: _SAD((((uint32_t)c->TB)<<16)|c->AA,(c->RR&0xFF)); _VDA(); _MLB(); _WR(); break; \
    case ((op)<<4)|8: _FETCH(); break;

/* implied-mode instruction: 2 cycles (op in the internal second cycle) */
#define _M_IMPLIED(op, body) \
    case ((op)<<4)|0: { body; } _DUMMY(); break; \
    case ((op)<<4)|1: _FETCH(); break;

/* read/modify/write on the accumulator */
#define _ACC_RMW(OP) do { _RMW_##OP(c->C); if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|(c->RR&0xFF)); } else { c->C=c->RR; } } while (0)

/*--- branches, jumps and subroutines ---*/

/* 8-bit relative branch: 2 cycles if not taken, 3 if taken (the 65816
   never adds a page-cross penalty for taken branches)
*/
#define _M_BRANCH(op, cond) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: { int8_t off=(int8_t)_GD(); \
        if (cond) { c->AD=(uint16_t)(c->PC+off); _DUMMY_PP(); \
                    if (!(c->E && (((c->PC^c->AD)&0xFF00)!=0))) { c->IR++; } } \
        else { _FETCH(); } } break; \
    case ((op)<<4)|2: _DUMMY_PP(); break; \
    case ((op)<<4)|3: c->PC=c->AD; _FETCH(); break;

/* 16-bit relative branch (BRL): 4 cycles */
#define _M_BRL(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t tgt=(uint16_t)(c->PC+(int16_t)(c->TA|(((uint16_t)_GD())<<8))); _DUMMY_PP(); c->PC=tgt; } break; \
    case ((op)<<4)|3: _FETCH(); break;

/* JMP abs: 3 cycles */
#define _M_JMP_ABS(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->PC=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _FETCH(); break;

/* JMP al (long): 4 cycles */
#define _M_JMP_AL(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->PBR=_GD(); c->PC=c->TB; _FETCH(); break;

/* JMP (abs): 5 cycles, the pointer high byte is fetched from the same
   256-byte page (NMOS-style wrap, per WDC docs; not covered by tests) */
#define _M_JMP_IND(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->AA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)(c->AA|(((uint16_t)_GD())<<8)); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->PC=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _FETCH(); break;

/* JMP (abs,X): 6 cycles, pointer in the program bank, X add done on a
   separate dummy cycle */
#define _M_JMP_INDX(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TA=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); c->AA=(uint16_t)(c->TA+c->X); _DUMMY_PP(); break; \
    case ((op)<<4)|3: _SA((((uint32_t)c->PBR)<<16)|c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TA=_GD(); _SA((((uint32_t)c->PBR)<<16)|((c->AA+1)&0xFFFF)); _VDA(); break; \
    case ((op)<<4)|5: c->PC=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _FETCH(); break;

/* JML (al): 6 cycles, the 24-bit pointer is fetched from bank zero */
#define _M_JML_IND(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->AA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)(c->AA|(((uint16_t)_GD())<<8)); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TB=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TB|=((uint16_t)_GD())<<8; _SA((c->AA+2)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|5: c->PBR=_GD(); c->PC=c->TB; _FETCH(); break;

/* JSR abs: 6 cycles, pushes PCH/PCL (return address = address of the
   last operand byte) */
#define _M_JSR_ABS(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); _VPA(); break; \
    case ((op)<<4)|2: c->TB=_GD(); _DUMMY(); break; \
    case ((op)<<4)|3: _SA(_SH()); _VDA(); _SD((uint8_t)(c->PC>>8)); _WR(); c->S=_SPADD(-1); break; \
    case ((op)<<4)|4: _SA(_SH()); _VDA(); _SD((uint8_t)c->PC); _WR(); c->S=_SPADD(-1); break; \
    case ((op)<<4)|5: c->PC=(uint16_t)(c->TA|(((uint16_t)c->TB)<<8)); _FETCH(); break;

/* JSL al: 8 cycles, pushes PBR/PCH/PCL (return = address of the bank byte).
   NOTE: the push addresses are computed linearly from the page-one stack
   pointer in emulation mode and can cross the page boundary (verified:
   an S of 0x0100 pushes to 0x0100/0x00FF/0x00FE, with S ending at 0x01FD),
   the S register itself is only updated once at the end */
#define _M_JSL(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=_GD(); _SA(_SH()); _VDA(); _SD(c->PBR); _WR(); c->AA=_SPLIN(-1); break; \
    case ((op)<<4)|3: _SA(_SPLIN(0)); break; \
    case ((op)<<4)|4: _SA(_PB_PC()); _VPA(); break; \
    case ((op)<<4)|5: { uint8_t bank=_GD(); _SA(c->AA); _VDA(); _SD((uint8_t)(c->PC>>8)); _WR(); c->AA=_SPLIN(-2); c->PBR=bank; } break; \
    case ((op)<<4)|6: _SA(c->AA); _VDA(); _SD((uint8_t)c->PC); _WR(); c->AA=_SPLIN(-3); c->S=_SPADD(-3); break; \
    case ((op)<<4)|7: c->PC=c->TA|(((uint16_t)c->TB)<<8); _FETCH(); break;

/* JSR (abs,X): 8 cycles, pushes the return address before fetching the
   pointer (which lives in the program bank) */
#define _M_JSR_INDX(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_SH()); _VDA(); _SD((uint8_t)(c->PC>>8)); _WR(); c->S=_SPADD(-1); break; \
    case ((op)<<4)|2: _SA(_SH()); _VDA(); _SD((uint8_t)c->PC); _WR(); c->S=_SPADD(-1); break; \
    case ((op)<<4)|3: _SA(_PB_PC()); _VPA(); c->PC++; break; \
    case ((op)<<4)|4: c->TB=_GD(); c->TA=(uint16_t)(c->TA|(((uint16_t)c->TB)<<8)); c->AA=(uint16_t)(c->TA+c->X); _DUMMY_PP(); break; \
    case ((op)<<4)|5: _SA((((uint32_t)c->PBR)<<16)|c->AA); _VDA(); break; \
    case ((op)<<4)|6: c->TA=_GD(); _SA((((uint32_t)c->PBR)<<16)|((c->AA+1)&0xFFFF)); _VDA(); break; \
    case ((op)<<4)|7: c->PC=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _FETCH(); break;

/* RTS: 6 cycles, pulled PC is incremented */
#define _M_RTS(op) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _DUMMY(); break; \
    case ((op)<<4)|2: c->AA=_SPADD(1); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); c->AA=_SPADD(2); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; c->S=c->AA; c->PC=(uint16_t)(c->TA+1); _SA(_SH()); break; \
    case ((op)<<4)|5: _FETCH(); break;

/*--- stack push/pull, PEA/PEI/PER ---*/

/* 8-bit push: 3 cycles */
#define _M_PUSH8(op, val) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _SA(_SH()); _VDA(); _SD((uint8_t)(val)); _WR(); c->S=_SPADD(-1); break; \
    case ((op)<<4)|2: _FETCH(); break;

/* 16-bit push: 4 cycles (linear addresses, can cross the page-1 boundary) */
#define _M_PUSH16_LIN(op, val) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _SA(_SPLIN(0)); _VDA(); _SD((((uint8_t)(((val)>>8)&0xFF)))); _WR(); break; \
    case ((op)<<4)|2: _SA(_SPLIN(-1)); _VDA(); _SD(((uint8_t)(val))); _WR(); c->S=_SPADD(-2); break; \
    case ((op)<<4)|3: _FETCH(); break;

/* width-aware push (8 or 16-bit) */
#define _M_PUSH_W(op, val, w8) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: if (w8) { _SA(_SH()); _VDA(); _SD((uint8_t)(val)); _WR(); c->S=_SPADD(-1); c->IR++; } else { _SA(_SPLIN(0)); _VDA(); _SD((uint8_t)(((val)>>8)&0xFF)); _WR(); } break; \
    case ((op)<<4)|2: if (w8) { CHIPS_ASSERT(false); } else { _SA(_SPLIN(-1)); _VDA(); _SD(((uint8_t)(val))); _WR(); c->S=_SPADD(-2); } break; \
    case ((op)<<4)|3: _FETCH(); break;

/* 8-bit pull (4 cycles), width-aware 16-bit variant (5 cycles) */
#define _PLA8(v) do { c->C=(uint16_t)((c->C&0xFF00)|((v)&0xFF)); _NZ8(c->C); } while (0)
#define _PLA16(v) do { c->C=(v); _NZ16(c->C); } while (0)
#define _PLP8(v) do { c->P=(uint8_t)(v); if (c->E) { c->P=(uint8_t)(c->P|(W65C816_MF|W65C816_XF)); } } while (0)
#define _PLX8(v) do { c->X=(uint16_t)((v)&0xFF); _NZ8(c->X); } while (0)
#define _PLX16(v) do { c->X=(v); _NZ16(c->X); } while (0)
#define _PLY8(v) do { c->Y=(uint16_t)((v)&0xFF); _NZ8(c->Y); } while (0)
#define _PLY16(v) do { c->Y=(v); _NZ16(c->Y); } while (0)
#define _M_PULL_W(op, load8, load16, w8) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _DUMMY(); break; \
    case ((op)<<4)|2: c->AA=_SPADD(1); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TD=_GD(); if (w8) { load8(c->TD); c->S=_SPADD(1); _FETCH(); } else { _SA(_SPADD(2)); _VDA(); } break; \
    case ((op)<<4)|4: c->TD|=((uint16_t)_GD())<<8; load16(c->TD); c->S=_SPADD(2); _FETCH(); break;

/* PLB: DBR pull, linear addresses in emulation mode */
#define _M_PLB(op) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _DUMMY(); break; \
    case ((op)<<4)|2: c->AA=_SPLIN(1); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->DBR=_GD(); _NZ8(c->DBR); c->S=_SPADD(1); _FETCH(); break;

/* PLD: 16-bit D pull with linear addresses */
#define _M_PLD(op) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _DUMMY(); break; \
    case ((op)<<4)|2: c->AA=_SPLIN(1); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TD=_GD(); c->AA=_SPLIN(2); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TD|=((uint16_t)_GD())<<8; c->D=c->TD; _NZ16(c->D); c->S=_SPADD(2); _FETCH(); break;

/* PEA: push 16-bit immediate operand: 5 cycles */
#define _M_PEA(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TA=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_SPLIN(0)); _VDA(); _SD((uint8_t)(c->TA>>8)); _WR(); break; \
    case ((op)<<4)|3: _SA(_SPLIN(-1)); _VDA(); _SD((uint8_t)c->TA); _WR(); c->S=_SPADD(-2); break; \
    case ((op)<<4)|4: _FETCH(); break;

/* PEI: push the 16-bit word at (dp): 6 cycles + 1 if D low byte != 0 */
#define _M_PEI(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (!(c->D&0xFF)) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _DUMMY_PP(); break; \
    case ((op)<<4)|2: c->AA=(uint16_t)((((c->D&0xFF)?c->TA:_GD())+c->D)&0xFFFF); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); _SA((c->AA+1)&0xFFFF); _VDA(); break; \
    case ((op)<<4)|4: c->TA=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _SA(_SPLIN(0)); _VDA(); _SD((uint8_t)(c->TA>>8)); _WR(); break; \
    case ((op)<<4)|5: _SA(_SPLIN(-1)); _VDA(); _SD((uint8_t)c->TA); _WR(); c->S=_SPADD(-2); break; \
    case ((op)<<4)|6: _FETCH(); break;

/* PER: push the 16-bit effective PC-relative address: 6 cycles */
#define _M_PER(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: { uint16_t tgt=(uint16_t)(c->PC+(int16_t)(c->TA|(((uint16_t)_GD())<<8))); _DUMMY_PP(); c->TA=tgt; } break; \
    case ((op)<<4)|3: _SA(_SPLIN(0)); _VDA(); _SD((uint8_t)(c->TA>>8)); _WR(); break; \
    case ((op)<<4)|4: _SA(_SPLIN(-1)); _VDA(); _SD((uint8_t)c->TA); _WR(); c->S=_SPADD(-2); break; \
    case ((op)<<4)|5: _FETCH(); break;

/*--- MVN/MVP block move ---
   7 cycles per byte, and the whole instruction re-executes for every
   byte (the traces show a full opcode+operand refetch per byte).
   X = source pointer, Y = destination pointer (offsets; the banks come
   from the two operand bytes, DBR is loaded with the destination bank),
   C = byte count (C+1 bytes are moved, 16-bit in native mode, 8-bit in
   emulation mode). MVP decrements X/Y after each byte, MVN increments.
---*/
#define _M_MVN(op, dec) \
    case ((op)<<4)|0: \
        _SA((((uint32_t)c->PBR)<<16)|(uint16_t)(c->PC-1)); _VDA(); _VPA(); break; \
    case ((op)<<4)|1: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: c->TB=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|3: c->RR=_GD(); c->DBR=c->TB; c->AD=((((uint32_t)c->TB)<<16)|c->Y); _SA(((((uint32_t)c->RR)<<16)|c->X)); _VDA(); break; \
    case ((op)<<4)|4: c->TD=_GD(); _SA(c->AD); _VDA(); _SD(c->TD); _WR(); break; \
    case ((op)<<4)|5: { const uint16_t mask=(_X8()?0xFF:0xFFFF); \
        c->X=(uint16_t)((dec?(c->X-1):(c->X+1))&mask); \
        c->Y=(uint16_t)((dec?(c->Y-1):(c->Y+1))&mask); } \
        _SA(c->AD); break; \
    case ((op)<<4)|6: _SA(c->AD); c->C=(uint16_t)(c->C-1); \
        if (c->C == 0xFFFF) { c->IR=((op)<<4)|7; } \
        else { c->PC=(uint16_t)(c->PC-2); c->IR=((op)<<4)|0; } break; \
    case ((op)<<4)|7: _FETCH(); break;

/*--- register transfers, XBA, XCE (2-cycle implied ops) ---*/
#define _TR_TAX() do { if (_X8()) { c->X=(uint16_t)(c->C&0xFF); _NZ8(c->X); } else { c->X=c->C; _NZ16(c->X); } } while (0)
#define _TR_TAY() do { if (_X8()) { c->Y=(uint16_t)(c->C&0xFF); _NZ8(c->Y); } else { c->Y=c->C; _NZ16(c->Y); } } while (0)
#define _TR_TXA() do { if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|(c->X&0xFF)); _NZ8(c->C); } else { c->C=c->X; _NZ16(c->C); } } while (0)
#define _TR_TYA() do { if (_W8()) { c->C=(uint16_t)((c->C&0xFF00)|(c->Y&0xFF)); _NZ8(c->C); } else { c->C=c->Y; _NZ16(c->C); } } while (0)
#define _TR_TSX() do { if (_X8()) { c->X=(uint16_t)(_SH()&0xFF); _NZ8(c->X); } else { c->X=_SH(); _NZ16(c->X); } } while (0)
#define _TR_TXS() do { c->S=c->E?(uint16_t)(0x0100|(c->X&0xFF)):c->X; } while (0)
#define _TR_TXY() do { if (_X8()) { c->Y=(uint16_t)(c->X&0xFF); _NZ8(c->Y); } else { c->Y=c->X; _NZ16(c->Y); } } while (0)
#define _TR_TYX() do { if (_X8()) { c->X=(uint16_t)(c->Y&0xFF); _NZ8(c->X); } else { c->X=c->Y; _NZ16(c->X); } } while (0)
#define _TR_TCS() do { c->S=c->E?(uint16_t)(0x0100|(c->C&0xFF)):c->C; } while (0)
#define _TR_TSC() do { c->C=_SH(); _NZ16(c->C); } while (0)
#define _TR_TCD() do { c->D=c->C; _NZ16(c->D); } while (0)
#define _TR_TDC() do { c->C=c->D; _NZ16(c->C); } while (0)
#define _TR_XBA() do { c->C=(uint16_t)((c->C>>8)|((c->C&0xFF)<<8)); _NZ8(c->C); } while (0)
#define _TR_XCE_PREP() do { c->E=(c->P&W65C816_CF)?1:0; if (c->E) { c->P|=(uint8_t)(W65C816_MF|W65C816_XF); c->S=(uint16_t)(0x0100|(c->S&0xFF)); c->X&=0xFF; c->Y&=0xFF; } } while (0)

/* SEP/REP: 3 cycles, M/X bits can not be modified in emulation mode */
/* NOTE: like XCE, the M/X flag state pins show the OLD flags during the
   internal cycle, the update becomes visible with the next fetch */
#define _M_IMM_FLAGS(op, set) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|1: { c->TA=_GD(); } _DUMMY_PP(); break; \
    case ((op)<<4)|2: { uint8_t m=(uint8_t)c->TA; \
        if (c->E) { m&=(uint8_t)~(W65C816_MF|W65C816_XF); } \
        if (set) { c->P=(uint8_t)(c->P|m); } else { c->P=(uint8_t)(c->P&~m); } \
        if (c->P&W65C816_XF) { c->X&=0xFF; c->Y&=0xFF; } } \
        _FETCH(); break;

/* immediate ops with X-flag width (LDX/LDY/CPX/CPY #) */
#define _LDX(v) do { if (_X8()) { c->X=(uint16_t)((v)&0xFF); _NZ8(c->X); } else { c->X=(v); _NZ16(c->X); } } while (0)
#define _LDY(v) do { if (_X8()) { c->Y=(uint16_t)((v)&0xFF); _NZ8(c->Y); } else { c->Y=(v); _NZ16(c->Y); } } while (0)
#define _CPX(v) do { if (_X8()) { uint32_t t=(uint32_t)(c->X&0xFF)-((v)&0xFF); _NZ8((uint16_t)t); if (0==(t&0x100)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } else { uint32_t t=(uint32_t)c->X-((v)&0xFFFF); _NZ16((uint16_t)t); if (0==(t&0x10000)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } } while (0)
#define _CPY(v) do { if (_X8()) { uint32_t t=(uint32_t)(c->Y&0xFF)-((v)&0xFF); _NZ8((uint16_t)t); if (0==(t&0x100)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } else { uint32_t t=(uint32_t)c->Y-((v)&0xFFFF); _NZ16((uint16_t)t); if (0==(t&0x10000)) { c->P|=W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } } } while (0)
#define _M_IMM_X(op, exec) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (_X8()) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: if (_X8()) { exec(_GD()); } else { exec((uint16_t)(c->TA|(((uint16_t)_GD())<<8))); } _FETCH(); break;

/* BIT memory: N/V from the operand, Z from A AND operand */
#define _BITM(v) do { if (_W8()) { uint8_t m=(uint8_t)(v); c->P=(uint8_t)((c->P&~(W65C816_NF|W65C816_VF|W65C816_ZF))|((m&0x80)?W65C816_NF:0)|((m&0x40)?W65C816_VF:0)|((((c->C&m)&0xFF)==0)?W65C816_ZF:0)); } else { uint16_t m=(uint16_t)(v); c->P=(uint8_t)((c->P&~(W65C816_NF|W65C816_VF|W65C816_ZF))|((m&0x8000)?W65C816_NF:0)|((m&0x4000)?W65C816_VF:0)|((((c->C&m)&0xFFFF)==0)?W65C816_ZF:0)); } } while (0)
/* INY/DEX/INX (index register width) */
#define _INY() do { if (_X8()) { c->Y=(uint16_t)((c->Y+1)&0xFF); _NZ8(c->Y); } else { c->Y=(uint16_t)((c->Y+1)&0xFFFF); _NZ16(c->Y); } } while (0)
#define _DEX() do { if (_X8()) { c->X=(uint16_t)((c->X-1)&0xFF); _NZ8(c->X); } else { c->X=(uint16_t)((c->X-1)&0xFFFF); _NZ16(c->X); } } while (0)
#define _INX() do { if (_X8()) { c->X=(uint16_t)((c->X+1)&0xFF); _NZ8(c->X); } else { c->X=(uint16_t)((c->X+1)&0xFFFF); _NZ16(c->X); } } while (0)

/* BIT immediate: only the Z flag is affected */
#define _M_BIT_IMM(op) \
    case ((op)<<4)|0: _SA(_PB_PC()); c->PC++; _VPA(); if (_W8()) { c->IR++; } break; \
    case ((op)<<4)|1: c->TA=_GD(); _SA(_PB_PC()); c->PC++; _VPA(); break; \
    case ((op)<<4)|2: if (_W8()) { c->P=(uint8_t)((c->P&~W65C816_ZF)|((((c->C&_GD())&0xFF)==0)?W65C816_ZF:0)); } else { c->P=(uint8_t)((c->P&~W65C816_ZF)|(((c->C&(uint16_t)(c->TA|(((uint16_t)_GD())<<8)))==0)?W65C816_ZF:0)); } _FETCH(); break;

/* RTL: 6 cycles, pulls PBR too, pulled PC is incremented */
/* NOTE: unlike RTS/RTI, RTL's pull addresses are computed linearly from the
   page-one stack pointer in emulation mode (verified against traces: an
   S of 0x01FF pulls from 0x0200..0x0202), the S register itself wraps */
#define _M_RTL(op) \
    case ((op)<<4)|0: _DUMMY(); break; \
    case ((op)<<4)|1: _DUMMY(); break; \
    case ((op)<<4)|2: c->AA=_SPLIN(1); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|3: c->TA=_GD(); c->AA=_SPLIN(2); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|4: c->TA|=((uint16_t)_GD())<<8; c->AA=_SPLIN(3); _SA(c->AA); _VDA(); break; \
    case ((op)<<4)|5: c->S=_SPADD(3); c->PBR=_GD(); c->PC=(uint16_t)(c->TA+1); _FETCH(); break;

/*--- interrupts, BRK, COP ---
   slot 0 (the signature-fetch cycle) is emitted by the caller; this tail
   pushes the return state and fetches the vector. In native mode PBR is
   pushed first, in emulation mode only PCH/PCL/P are pushed. The pushed
   status byte is the unmodified P (verified against traces), afterwards
   D is cleared and I is set. The vector is fetched from bank zero with
   VDA|VPB asserted and PBR is cleared.
   On RESET all push cycles are suppressed and the CPU state is reset.
---*/
#define _W65C816_BRK_TAIL(op) \
    case ((op)<<4)|1: if (c->E) { _SA(_SH()); _VDA(); _SD((uint8_t)(c->PC>>8)); if (!(c->brk_flags&W65C816_BRK_RESET)) { _WR(); } c->S=_SPADD(-1); c->IR++; } else { _SA(_SH()); _VDA(); _SD(c->PBR); if (!(c->brk_flags&W65C816_BRK_RESET)) { _WR(); } c->S=_SPADD(-1); } break; \
    case ((op)<<4)|2: _SA(_SH()); _VDA(); _SD((uint8_t)(c->PC>>8)); if (!(c->brk_flags&W65C816_BRK_RESET)) { _WR(); } c->S=_SPADD(-1); break; \
    case ((op)<<4)|3: _SA(_SH()); _VDA(); _SD((uint8_t)c->PC); if (!(c->brk_flags&W65C816_BRK_RESET)) { _WR(); } c->S=_SPADD(-1); break; \
    case ((op)<<4)|4: { uint16_t vec; _SA(_SH()); _VDA(); _SD(c->P); if (!(c->brk_flags&W65C816_BRK_RESET)) { _WR(); } c->S=_SPADD(-1); \
        if (c->brk_flags&W65C816_BRK_RESET) { vec=0xFFFC; } \
        else if (c->brk_flags&W65C816_BRK_NMI) { vec=c->E?0xFFFA:0xFFEA; } \
        else if (c->brk_flags&W65C816_BRK_ABORT) { vec=0xFFF8; } \
        else if (c->brk_flags&W65C816_BRK_IRQ) { vec=c->E?0xFFFE:0xFFEE; } \
        else if ((op)==0x00) { vec=c->E?0xFFFE:0xFFE6; } \
        else { vec=c->E?0xFFF4:0xFFE4; } \
        c->AD=vec; c->PBR=0; \
        if (c->brk_flags&W65C816_BRK_RESET) { c->E=1; c->P|=(uint8_t)(W65C816_IF|W65C816_DF|W65C816_MF|W65C816_XF); c->DBR=0; c->D=0; c->S=0x01FF; c->X&=0xFF; c->Y&=0xFF; } \
        else { c->P=(uint8_t)((c->P&~W65C816_DF)|W65C816_IF); } } break; \
    case ((op)<<4)|5: _SA(c->AD); _VDA(); _VPB(); break; \
    case ((op)<<4)|6: c->TA=_GD(); _SA((c->AD+1)&0xFFFF); _VDA(); _VPB(); break; \
    case ((op)<<4)|7: c->PC=(uint16_t)(c->TA|(((uint16_t)_GD())<<8)); _FETCH(); break;

/* placeholder for not-yet implemented opcodes:
   keeps the CPU jammed on the same microstep (reads at PB:PC) so that
   a test harness fails fast instead of running away.
*/
#define _W65C816_TODO(op) \
    case ((op)<<4)|0: case ((op)<<4)|1: case ((op)<<4)|2: case ((op)<<4)|3: \
    case ((op)<<4)|4: case ((op)<<4)|5: case ((op)<<4)|6: case ((op)<<4)|7: \
    case ((op)<<4)|8: case ((op)<<4)|9: case ((op)<<4)|10: case ((op)<<4)|11: \
    case ((op)<<4)|12: case ((op)<<4)|13: case ((op)<<4)|14: case ((op)<<4)|15: \
        c->IR--; _DUMMY(); break;

uint64_t w65c816_init(w65c816_t* c, const w65c816_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    memset(c, 0, sizeof(*c));
    /* reset state (see WDC datasheet): emulation mode, 8-bit M/X flags,
       IRQ and decimal flags set, stack pointer on page 1
    */
    c->E = 1;
    c->P = W65C816_IF | W65C816_DF | W65C816_MF | W65C816_XF;
    c->S = 0x01FF;
    c->PINS = W65C816_RW | W65C816_SYNC | W65C816_VPA | W65C816_VDA | W65C816_RES;
    return c->PINS;
}

void w65c816_snapshot_onsave(w65c816_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    /* no pointer fixups needed */
}

void w65c816_snapshot_onload(w65c816_t* snapshot, w65c816_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    (void)sys;
    /* no pointer fixups needed */
}

/* ADC helper (8/16-bit, binary and decimal mode).
   Decimal mode (fitted against SingleStepTests traces):
   - result is a chained BCD add (two digit pairs for 16-bit)
   - C comes from the final BCD carry
   - N and Z come from the BCD result
   - V = ~((A^M) & sign) & (A ^ (raw high nibble sum << shift)), i.e. it is
     computed like binary mode but with the raw (pre-adjust) high nibble
     sum instead of the binary result
*/
static inline void _w65c816_adc(w65c816_t* c, uint16_t v, bool w16) {
    const bool cf = 0 != (c->P & W65C816_CF);
    const int ci = cf ? 1 : 0;
    if (!w16) {
        const uint16_t a = c->C & 0xFF;
        const uint16_t bin = (uint16_t)(a + (v & 0xFF) + ci);
        c->P &= (uint8_t)~(W65C816_CF|W65C816_ZF|W65C816_VF|W65C816_NF);
        if (c->P & W65C816_DF) {
            int al = (a & 0x0F) + (v & 0x0F) + ci;
            if (al > 9) { al += 6; }
            int ah = (a >> 4) + (v >> 4) + (al > 0x0F);
            const int ahr = ah;
            if (ah > 9) { ah += 6; }
            const uint16_t r = (uint16_t)(((ah & 0x0F) << 4) | (al & 0x0F));
            if (ahr > 9) { c->P |= W65C816_CF; }
            if (!(r & 0xFF)) { c->P |= W65C816_ZF; }
            if (~(a ^ v) & (a ^ (ahr << 4)) & 0x80) { c->P |= W65C816_VF; }
            if (r & 0x80) { c->P |= W65C816_NF; }
            c->C = (uint16_t)((c->C & 0xFF00) | r);
        }
        else {
            if (bin & 0x100) { c->P |= W65C816_CF; }
            if (!(bin & 0xFF)) { c->P |= W65C816_ZF; }
            if (~(a ^ v) & (a ^ bin) & 0x80) { c->P |= W65C816_VF; }
            if (bin & 0x80) { c->P |= W65C816_NF; }
            c->C = (uint16_t)((c->C & 0xFF00) | (bin & 0xFF));
        }
    }
    else {
        const uint32_t a = c->C;
        const uint32_t bin = a + v + ci;
        c->P &= (uint8_t)~(W65C816_CF|W65C816_ZF|W65C816_VF|W65C816_NF);
        if (c->P & W65C816_DF) {
            int al = (a & 0x0F) + (v & 0x0F) + ci;
            if (al > 9) { al += 6; }
            int ah = ((a >> 4) & 0x0F) + ((v >> 4) & 0x0F) + (al > 0x0F);
            if (ah > 9) { ah += 6; }
            const int c8 = ah > 0x0F;
            const uint32_t lo = (uint32_t)(((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFF;
            al = ((a >> 8) & 0x0F) + ((v >> 8) & 0x0F) + c8;
            if (al > 9) { al += 6; }
            int ahr = ((a >> 12) & 0x0F) + ((v >> 12) & 0x0F) + (al > 0x0F);
            ah = ahr;
            if (ah > 9) { ah += 6; }
            const uint16_t r = (uint16_t)((((((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFF) << 8) | lo);
            if (ahr > 9) { c->P |= W65C816_CF; }
            if (!(r & 0xFFFF)) { c->P |= W65C816_ZF; }
            if (~(a ^ v) & (a ^ (ahr << 12)) & 0x8000) { c->P |= W65C816_VF; }
            if (r & 0x8000) { c->P |= W65C816_NF; }
            c->C = r;
        }
        else {
            if (bin & 0x10000) { c->P |= W65C816_CF; }
            if (!(bin & 0xFFFF)) { c->P |= W65C816_ZF; }
            if (~(a ^ v) & (a ^ bin) & 0x8000) { c->P |= W65C816_VF; }
            if (bin & 0x8000) { c->P |= W65C816_NF; }
            c->C = (uint16_t)(bin & 0xFFFF);
        }
    }
}

/* SBC helper (8/16-bit, binary and decimal mode).
   Decimal mode (fitted against SingleStepTests traces):
   - result is a chained BCD subtract (two digit pairs for 16-bit)
   - C and V come from the binary subtraction
   - N and Z come from the BCD result
*/
static inline void _w65c816_sbc(w65c816_t* c, uint16_t v, bool w16) {
    const bool nb = 0 == (c->P & W65C816_CF);   /* borrow when carry clear */
    const int bi = nb ? 1 : 0;
    if (!w16) {
        const uint16_t a = c->C & 0xFF;
        const uint16_t vv = v & 0xFF;
        const uint16_t bin = (uint16_t)(a - vv - bi);
        c->P &= (uint8_t)~(W65C816_CF|W65C816_ZF|W65C816_VF|W65C816_NF);
        if (c->P & W65C816_DF) {
            int al = (a & 0x0F) - (vv & 0x0F) - bi;
            int ah = (a >> 4) - (vv >> 4);
            if (al < 0) { ah--; al -= 6; }
            if (ah < 0) { ah -= 6; }
            const uint16_t r = (uint16_t)((((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFF);
            if (!(bin & 0xFF00)) { c->P |= W65C816_CF; }
            if (!(r & 0xFF)) { c->P |= W65C816_ZF; }
            if ((a ^ vv) & (a ^ bin) & 0x80) { c->P |= W65C816_VF; }
            if (r & 0x80) { c->P |= W65C816_NF; }
            c->C = (uint16_t)((c->C & 0xFF00) | r);
        }
        else {
            if (!(bin & 0xFF00)) { c->P |= W65C816_CF; }
            if (!(bin & 0xFF)) { c->P |= W65C816_ZF; }
            if ((a ^ vv) & (a ^ bin) & 0x80) { c->P |= W65C816_VF; }
            if (bin & 0x80) { c->P |= W65C816_NF; }
            c->C = (uint16_t)((c->C & 0xFF00) | (bin & 0xFF));
        }
    }
    else {
        const uint32_t a = c->C;
        const uint32_t vv = v;
        const uint32_t bin = a - vv - bi;
        c->P &= (uint8_t)~(W65C816_CF|W65C816_ZF|W65C816_VF|W65C816_NF);
        if (c->P & W65C816_DF) {
            int al = (a & 0x0F) - (vv & 0x0F) - bi;
            int ah = ((a >> 4) & 0x0F) - ((vv >> 4) & 0x0F);
            if (al < 0) { ah--; al -= 6; }
            if (ah < 0) { ah -= 6; }
            const int b8 = ah < 0;
            const uint32_t lo = (uint32_t)(((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFF;
            al = ((a >> 8) & 0x0F) - ((vv >> 8) & 0x0F) - b8;
            ah = ((a >> 12) & 0x0F) - ((vv >> 12) & 0x0F);
            if (al < 0) { ah--; al -= 6; }
            if (ah < 0) { ah -= 6; }
            const uint16_t r = (uint16_t)((((((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFF) << 8) | lo);
            if (!(bin & 0x10000)) { c->P |= W65C816_CF; }
            if (!(r & 0xFFFF)) { c->P |= W65C816_ZF; }
            if ((a ^ vv) & (a ^ bin) & 0x8000) { c->P |= W65C816_VF; }
            if (r & 0x8000) { c->P |= W65C816_NF; }
            c->C = r;
        }
        else {
            if (!(bin & 0x10000)) { c->P |= W65C816_CF; }
            if (!(bin & 0xFFFF)) { c->P |= W65C816_ZF; }
            if ((a ^ vv) & (a ^ bin) & 0x8000) { c->P |= W65C816_VF; }
            if (bin & 0x8000) { c->P |= W65C816_NF; }
            c->C = (uint16_t)(bin & 0xFFFF);
        }
    }
}

uint64_t w65c816_tick(w65c816_t* c, uint64_t pins) {
    if (c->halted) {
        /* WAI wakes on RES, NMI, ABORT and (unmasked) IRQ, STP only on RES */
        bool wake = false;
        if (0 != (pins & W65C816_RES)) {
            wake = true;
            c->brk_flags |= W65C816_BRK_RESET;
        }
        else if (c->halted != 2) {
            if (0 != ((pins & (pins ^ c->PINS)) & (W65C816_NMI))) {
                wake = true; c->brk_flags |= W65C816_BRK_NMI;
            }
            if (0 != ((pins & (pins ^ c->PINS)) & (W65C816_ABORT))) {
                wake = true; c->brk_flags |= W65C816_BRK_ABORT;
            }
            if (!wake && (pins & W65C816_IRQ) && (0 == (c->P & W65C816_IF))) {
                wake = true; c->brk_flags |= W65C816_BRK_IRQ;
            }
        }
        if (!wake) {
            _STATUS_PINS();
            c->PINS = pins;
            return pins;
        }
        c->halted = 0;
        c->IR = 0;      /* enter the interrupt sequence */
        _OFF(W65C816_SYNC);
        _OFF(W65C816_VPA|W65C816_VDA|W65C816_VPB|W65C816_MLB);
        _RD();
        switch (c->IR++) {
            case 0: _SA(_PB_PC()); _VPA(); break;
            _W65C816_BRK_TAIL(0x00)
            default: CHIPS_ASSERT(false); break;
        }
        _STATUS_PINS();
        c->PINS = pins;
        c->irq_pip <<= 1; c->nmi_pip <<= 1; c->abrt_pip <<= 1;
        return pins;
    }
    if (pins & (W65C816_SYNC|W65C816_IRQ|W65C816_NMI|W65C816_ABORT|W65C816_RDY|W65C816_RES)) {
        // interrupt detection also works in RDY phases, but only NMI is "sticky"

        // NMI is edge-triggered
        if (0 != ((pins & (pins ^ c->PINS)) & W65C816_NMI)) {
            c->nmi_pip |= 0x100;
        }
        // ABORT is edge-triggered
        if (0 != ((pins & (pins ^ c->PINS)) & W65C816_ABORT)) {
            c->abrt_pip |= 0x100;
        }
        // IRQ test is level triggered
        if ((pins & W65C816_IRQ) && (0 == (c->P & W65C816_IF))) {
            c->irq_pip |= 0x100;
        }

        // RDY pin is only checked during read cycles
        if ((pins & (W65C816_RW|W65C816_RDY)) == (W65C816_RW|W65C816_RDY)) {
            _STATUS_PINS();
            c->PINS = pins;
            c->irq_pip <<= 1;
            c->abrt_pip <<= 1;
            return pins;
        }
        if (pins & W65C816_SYNC) {
            // load new instruction into 'instruction register' and restart tick counter
            c->IR = ((uint16_t)_GD())<<4;
            _OFF(W65C816_SYNC);

            // check IRQ, NMI, ABORT and RES state
            if (0 != (c->irq_pip & 0x400)) {
                c->brk_flags |= W65C816_BRK_IRQ;
            }
            if (0 != (c->nmi_pip & 0xFC00)) {
                c->brk_flags |= W65C816_BRK_NMI;
            }
            if (0 != (c->abrt_pip & 0x400)) {
                c->brk_flags |= W65C816_BRK_ABORT;
            }
            if (0 != (pins & W65C816_RES)) {
                c->brk_flags |= W65C816_BRK_RESET;
            }
            c->irq_pip &= 0x3FF;
            c->nmi_pip &= 0x3FF;
            c->abrt_pip &= 0x3FF;

            // if interrupt or reset was requested, force a BRK sequence
            // (the BRK implementation decides what to push and which
            // vector to fetch depending on brk_flags and mode)
            if (c->brk_flags) {
                c->IR = 0;
            }
            else {
                c->PC++;
                // MVN/MVP start at microstep 1: the opcode prefetch of the
                // previous cycle substitutes the per-byte refetch
                if (((c->IR>>4)==0x44)||((c->IR>>4)==0x54)) { c->IR |= 1; }
            }
        }
    }
    // each microstep explicitly declares its own bus state, so clear
    // all 'valid bus' pins that may have been set by the previous cycle
    // (the opcode fetch cycle of the previous instruction)
    _OFF(W65C816_VPA|W65C816_VDA|W65C816_VPB|W65C816_MLB);
    // reads are default, writes are special
    _RD();
    switch (c->IR++) {
    /* CLC */
        case (0x18<<4)|0: c->P &= ~W65C816_CF; _DUMMY(); break;
        case (0x18<<4)|1: _FETCH(); break;
    /* BRK (also the entry point for hardware interrupts; the signature
       byte is only fetched-and-skipped for software BRK) */
        case (0x00<<4)|0: _SA(_PB_PC()); _VPA(); if (0 == c->brk_flags) { c->PC++; } else { c->brk_flags = 0; } break; \
        _W65C816_BRK_TAIL(0x00)
    /* ORA (dp,X) */
        _M_IDX_RD(0x01, _ORA)
    /* COP */
        case (0x02<<4)|0: _SA(_PB_PC()); _VPA(); c->PC++; break; \
        _W65C816_BRK_TAIL(0x02)
    /* ORA sr,S */
        _M_SR_RD(0x03, _ORA)
    /* TSB dp */
        _M_DP_RMW(0x04, _RMW_TSB)
    /* ORA dp */
        _M_DP_RD(0x05, _ORA)
    /* ASL dp */
        _M_DP_RMW(0x06, _RMW_ASL)
    /* ORA [dp] */
        _M_IDL_RD(0x07, _ORA)
    /* PHP */
        _M_PUSH8(0x08, c->P)
    /* ORA # */
        _M_IMM_RD(0x09, _ORA)
    /* ASL A */
        _M_IMPLIED(0x0A, _ACC_RMW(ASL);)
    /* PHD */
        _M_PUSH16_LIN(0x0B, c->D)
    /* TSB abs */
        _M_ABS_RMW(0x0C, _RMW_TSB)
    /* ORA abs */
        _M_ABS_RD(0x0D, _ORA)
    /* ASL abs */
        _M_ABS_RMW(0x0E, _RMW_ASL)
    /* ORA al */
        _M_ABL_RD(0x0F, _ORA)
    /* BPL */
        _M_BRANCH(0x10, !(c->P&W65C816_NF))
    /* ORA (dp),Y */
        _M_IDY_RD(0x11, _ORA)
    /* ORA (dp) */
        _M_DPIND_RD(0x12, _ORA)
    /* ORA (sr,S),Y */
        _M_SRIY_RD(0x13, _ORA)
    /* TRB dp */
        _M_DP_RMW(0x14, _RMW_TRB)
    /* ORA dp,X */
        _M_DPXY_RD(0x15, _ORA, c->X)
    /* ASL dp,X */
        _M_DPXY_RMW(0x16, _RMW_ASL, c->X)
    /* ORA [dp],Y */
        _M_IDLY_RD(0x17, _ORA)
    /* ORA abs,Y */
        _M_ABXY_RD(0x19, _ORA, c->Y)
    /* INC A */
        _M_IMPLIED(0x1A, _ACC_RMW(INC);)
    /* TCS */
        _M_IMPLIED(0x1B, _TR_TCS();)
    /* TRB abs */
        _M_ABS_RMW(0x1C, _RMW_TRB)
    /* ORA abs,X */
        _M_ABXY_RD(0x1D, _ORA, c->X)
    /* ASL abs,X */
        _M_ABXY_RMW(0x1E, _RMW_ASL, c->X)
    /* ORA al,X */
        _M_ABLX_RD(0x1F, _ORA)
    /* JSR abs */
        _M_JSR_ABS(0x20)
    /* AND (dp,X) */
        _M_IDX_RD(0x21, _AND)
    /* JSL al */
        _M_JSL(0x22)
    /* AND sr,S */
        _M_SR_RD(0x23, _AND)
    /* BIT dp */
        _M_DP_RD(0x24, _BITM)
    /* AND dp */
        _M_DP_RD(0x25, _AND)
    /* ROL dp */
        _M_DP_RMW(0x26, _RMW_ROL)
    /* AND [dp] */
        _M_IDL_RD(0x27, _AND)
    /* PLP (always 8 bits; M/X flags can not be cleared in emulation mode) */
        case (0x28<<4)|0: _DUMMY(); break; \
        case (0x28<<4)|1: _DUMMY(); break; \
        case (0x28<<4)|2: c->AA=_SPADD(1); _SA(c->AA); _VDA(); break; \
        case (0x28<<4)|3: c->P=_GD(); if (c->E) { c->P=(uint8_t)(c->P|(W65C816_MF|W65C816_XF)); } if (c->P&W65C816_XF) { c->X&=0xFF; c->Y&=0xFF; } c->S=_SPADD(1); _FETCH(); break;
    /* AND # */
        _M_IMM_RD(0x29, _AND)
    /* ROL A */
        _M_IMPLIED(0x2A, _ACC_RMW(ROL);)
    /* PLD */
        _M_PLD(0x2B)
    /* BIT abs */
        _M_ABS_RD(0x2C, _BITM)
    /* AND abs */
        _M_ABS_RD(0x2D, _AND)
    /* ROL abs */
        _M_ABS_RMW(0x2E, _RMW_ROL)
    /* AND al */
        _M_ABL_RD(0x2F, _AND)
    /* BMI */
        _M_BRANCH(0x30, (c->P&W65C816_NF))
    /* AND (dp),Y */
        _M_IDY_RD(0x31, _AND)
    /* AND (dp) */
        _M_DPIND_RD(0x32, _AND)
    /* AND (sr,S),Y */
        _M_SRIY_RD(0x33, _AND)
    /* BIT dp,X */
        _M_DPXY_RD(0x34, _BITM, c->X)
    /* AND dp,X */
        _M_DPXY_RD(0x35, _AND, c->X)
    /* ROL dp,X */
        _M_DPXY_RMW(0x36, _RMW_ROL, c->X)
    /* AND [dp],Y */
        _M_IDLY_RD(0x37, _AND)
    /* SEC */
        _M_IMPLIED(0x38, c->P|=(uint8_t)W65C816_CF;)
    /* AND abs,Y */
        _M_ABXY_RD(0x39, _AND, c->Y)
    /* DEC A */
        _M_IMPLIED(0x3A, _ACC_RMW(DEC);)
    /* TSC */
        _M_IMPLIED(0x3B, _TR_TSC();)
    /* BIT abs,X */
        _M_ABXY_RD(0x3C, _BITM, c->X)
    /* AND abs,X */
        _M_ABXY_RD(0x3D, _AND, c->X)
    /* ROL abs,X */
        _M_ABXY_RMW(0x3E, _RMW_ROL, c->X)
    /* AND al,X */
        _M_ABLX_RD(0x3F, _AND)
    /* RTI: pulls P, PCL, PCH and (native mode only) PBR. NOTE: like XCE,
       the pulled P must not change the M/X state pins until the next bus
       cycle, so the register update happens one cycle after the pull */
        case (0x40<<4)|0: _DUMMY(); break; \
        case (0x40<<4)|1: _DUMMY(); break; \
        case (0x40<<4)|2: c->AA=_SPADD(1); _SA(c->AA); _VDA(); break; \
        case (0x40<<4)|3: c->TD=_GD(); c->AA=_SPADD(2); _SA(c->AA); _VDA(); break; \
        case (0x40<<4)|4: c->TA=_GD(); c->AA=_SPADD(3); _SA(c->AA); _VDA(); break; \
        case (0x40<<4)|5: c->TA|=((uint16_t)_GD())<<8; c->PC=c->TA; if (c->E) { c->P=c->TD; c->P=(uint8_t)(c->P|(W65C816_MF|W65C816_XF)); if (c->P&W65C816_XF) { c->X&=0xFF; c->Y&=0xFF; } c->S=_SPADD(3); _FETCH(); } else { c->AA=_SPADD(4); _SA(c->AA); _VDA(); } break; \
        case (0x40<<4)|6: c->P=c->TD; if (c->P&W65C816_XF) { c->X&=0xFF; c->Y&=0xFF; } c->PBR=_GD(); c->S=c->AA; _FETCH(); break;
    /* EOR (dp,X) */
        _M_IDX_RD(0x41, _EOR)
    /* WDM (2 bytes: the second byte is fetched and skipped) */
        case (0x42<<4)|0: _DUMMY(); c->PC++; break; \
        case (0x42<<4)|1: _FETCH(); break;
    /* EOR sr,S */
        _M_SR_RD(0x43, _EOR)
    /* MVP */
        _M_MVN(0x44, 1)
    /* EOR dp */
        _M_DP_RD(0x45, _EOR)
    /* LSR dp */
        _M_DP_RMW(0x46, _RMW_LSR)
    /* EOR [dp] */
        _M_IDL_RD(0x47, _EOR)
    /* PHA */
        _M_PUSH_W(0x48, c->C, _W8())
    /* EOR # */
        _M_IMM_RD(0x49, _EOR)
    /* LSR A */
        _M_IMPLIED(0x4A, _ACC_RMW(LSR);)
    /* PHK */
        _M_PUSH8(0x4B, c->PBR)
    /* JMP abs */
        _M_JMP_ABS(0x4C)
    /* EOR abs */
        _M_ABS_RD(0x4D, _EOR)
    /* LSR abs */
        _M_ABS_RMW(0x4E, _RMW_LSR)
    /* EOR al */
        _M_ABL_RD(0x4F, _EOR)
    /* BVC */
        _M_BRANCH(0x50, !(c->P&W65C816_VF))
    /* EOR (dp),Y */
        _M_IDY_RD(0x51, _EOR)
    /* EOR (dp) */
        _M_DPIND_RD(0x52, _EOR)
    /* EOR (sr,S),Y */
        _M_SRIY_RD(0x53, _EOR)
    /* MVN */
        _M_MVN(0x54, 0)
    /* EOR dp,X */
        _M_DPXY_RD(0x55, _EOR, c->X)
    /* LSR dp,X */
        _M_DPXY_RMW(0x56, _RMW_LSR, c->X)
    /* EOR [dp],Y */
        _M_IDLY_RD(0x57, _EOR)
    /* CLI */
        _M_IMPLIED(0x58, c->P&=(uint8_t)~W65C816_IF;)
    /* EOR abs,Y */
        _M_ABXY_RD(0x59, _EOR, c->Y)
    /* PHY */
        _M_PUSH_W(0x5A, c->Y, _X8())
    /* TCD */
        _M_IMPLIED(0x5B, _TR_TCD();)
    /* JMP al */
        _M_JMP_AL(0x5C)
    /* EOR abs,X */
        _M_ABXY_RD(0x5D, _EOR, c->X)
    /* LSR abs,X */
        _M_ABXY_RMW(0x5E, _RMW_LSR, c->X)
    /* EOR al,X */
        _M_ABLX_RD(0x5F, _EOR)
    /* RTS */
        _M_RTS(0x60)
    /* ADC (dp,X) */
        _M_IDX_RD(0x61, _ADC)
    /* PER */
        _M_PER(0x62)
    /* ADC sr,S */
        _M_SR_RD(0x63, _ADC)
    /* STZ dp */
        _M_DP_WR(0x64, 0, _W8())
    /* ADC dp */
        _M_DP_RD(0x65, _ADC)
    /* ROR dp */
        _M_DP_RMW(0x66, _RMW_ROR)
    /* ADC [dp] */
        _M_IDL_RD(0x67, _ADC)
    /* PLA */
        _M_PULL_W(0x68, _PLA8, _PLA16, _W8())
    /* ADC # */
        _M_IMM_RD(0x69, _ADC)
    /* ROR A */
        _M_IMPLIED(0x6A, _ACC_RMW(ROR);)
    /* RTL */
        _M_RTL(0x6B)
    /* JMP (abs) */
        _M_JMP_IND(0x6C)
    /* ADC abs */
        _M_ABS_RD(0x6D, _ADC)
    /* ROR abs */
        _M_ABS_RMW(0x6E, _RMW_ROR)
    /* ADC al */
        _M_ABL_RD(0x6F, _ADC)
    /* BVS */
        _M_BRANCH(0x70, (c->P&W65C816_VF))
    /* ADC (dp),Y */
        _M_IDY_RD(0x71, _ADC)
    /* ADC (dp) */
        _M_DPIND_RD(0x72, _ADC)
    /* ADC (sr,S),Y */
        _M_SRIY_RD(0x73, _ADC)
    /* STZ dp,X */
        _M_DPXY_WR(0x74, 0, _W8(), c->X)
    /* ADC dp,X */
        _M_DPXY_RD(0x75, _ADC, c->X)
    /* ROR dp,X */
        _M_DPXY_RMW(0x76, _RMW_ROR, c->X)
    /* ADC [dp],Y */
        _M_IDLY_RD(0x77, _ADC)
    /* SEI */
        _M_IMPLIED(0x78, c->P|=(uint8_t)W65C816_IF;)
    /* ADC abs,Y */
        _M_ABXY_RD(0x79, _ADC, c->Y)
    /* PLY */
        _M_PULL_W(0x7A, _PLY8, _PLY16, _X8())
    /* TDC */
        _M_IMPLIED(0x7B, _TR_TDC();)
    /* JMP (abs,X) */
        _M_JMP_INDX(0x7C)
    /* ADC abs,X */
        _M_ABXY_RD(0x7D, _ADC, c->X)
    /* ROR abs,X */
        _M_ABXY_RMW(0x7E, _RMW_ROR, c->X)
    /* ADC al,X */
        _M_ABLX_RD(0x7F, _ADC)
    /* BRA */
        _M_BRANCH(0x80, 1)
    /* STA (dp,X) */
        _M_IDX_WR(0x81, c->C, _W8())
    /* BRL */
        _M_BRL(0x82)
    /* STA sr,S */
        _M_SR_WR(0x83, c->C, _W8())
    /* STY dp */
        _M_DP_WR(0x84, c->Y, _X8())
    /* STA dp */
        _M_DP_WR(0x85, c->C, _W8())
    /* STX dp */
        _M_DP_WR(0x86, c->X, _X8())
    /* STA [dp] */
        _M_IDL_WR(0x87, c->C, _W8())
    /* DEY */
        _M_IMPLIED(0x88, do { if (_X8()) { c->Y=(uint16_t)((c->Y-1)&0xFF); _NZ8(c->Y); } else { c->Y=(uint16_t)((c->Y-1)&0xFFFF); _NZ16(c->Y); } } while (0);)
    /* BIT # */
        _M_BIT_IMM(0x89)
    /* TXA */
        _M_IMPLIED(0x8A, _TR_TXA();)
    /* PHB */
        _M_PUSH8(0x8B, c->DBR)
    /* STY abs */
        _M_ABS_WR(0x8C, c->Y, _X8())
    /* STA abs */
        _M_ABS_WR(0x8D, c->C, _W8())
    /* STX abs */
        _M_ABS_WR(0x8E, c->X, _X8())
    /* STA al */
        _M_ABL_WR(0x8F, c->C, _W8())
    /* BCC */
        _M_BRANCH(0x90, !(c->P&W65C816_CF))
    /* STA (dp),Y */
        _M_IDY_WR(0x91, c->C, _W8())
    /* STA (dp) */
        _M_DPIND_WR(0x92, c->C, _W8())
    /* STA (sr,S),Y */
        _M_SRIY_WR(0x93, c->C, _W8())
    /* STY dp,X */
        _M_DPXY_WR(0x94, c->Y, _X8(), c->X)
    /* STA dp,X */
        _M_DPXY_WR(0x95, c->C, _W8(), c->X)
    /* STX dp,Y */
        _M_DPXY_WR(0x96, c->X, _X8(), c->Y)
    /* STA [dp],Y */
        _M_IDLY_WR(0x97, c->C, _W8())
    /* TYA */
        _M_IMPLIED(0x98, _TR_TYA();)
    /* STA abs,Y */
        _M_ABXY_WR(0x99, c->C, _W8(), c->Y)
    /* TXS */
        _M_IMPLIED(0x9A, _TR_TXS();)
    /* TXY */
        _M_IMPLIED(0x9B, _TR_TXY();)
    /* STZ abs */
        _M_ABS_WR(0x9C, 0, _W8())
    /* STA abs,X */
        _M_ABXY_WR(0x9D, c->C, _W8(), c->X)
    /* STZ abs,X */
        _M_ABXY_WR(0x9E, 0, _W8(), c->X)
    /* STA al,X */
        _M_ABLX_WR(0x9F, c->C, _W8())
    /* LDY # */
        _M_IMM_X(0xA0, _LDY)
    /* LDA (dp,X) */
        _M_IDX_RD(0xA1, _LDA)
    /* LDX # */
        _M_IMM_X(0xA2, _LDX)
    /* LDA sr,S */
        _M_SR_RD(0xA3, _LDA)
    /* LDY dp */
        _M_DP_RDX(0xA4, _LDY)
    /* LDA dp */
        _M_DP_RD(0xA5, _LDA)
    /* LDX dp */
        _M_DP_RDX(0xA6, _LDX)
    /* LDA [dp] */
        _M_IDL_RD(0xA7, _LDA)
    /* TAY */
        _M_IMPLIED(0xA8, _TR_TAY();)
    /* LDA # */
        _M_IMM_RD(0xA9, _LDA)
    /* TAX */
        _M_IMPLIED(0xAA, _TR_TAX();)
    /* PLB */
        _M_PLB(0xAB)
    /* LDY abs */
        _M_ABS_RDX(0xAC, _LDY)
    /* LDA abs */
        _M_ABS_RD(0xAD, _LDA)
    /* LDX abs */
        _M_ABS_RDX(0xAE, _LDX)
    /* LDA al */
        _M_ABL_RD(0xAF, _LDA)
    /* BCS */
        _M_BRANCH(0xB0, (c->P&W65C816_CF))
    /* LDA (dp),Y */
        _M_IDY_RD(0xB1, _LDA)
    /* LDA (dp) */
        _M_DPIND_RD(0xB2, _LDA)
    /* LDA (sr,S),Y */
        _M_SRIY_RD(0xB3, _LDA)
    /* LDY dp,X */
        _M_DPXY_RDX(0xB4, _LDY, c->X)
    /* LDA dp,X */
        _M_DPXY_RD(0xB5, _LDA, c->X)
    /* LDX dp,Y */
        _M_DPXY_RDX(0xB6, _LDX, c->Y)
    /* LDA [dp],Y */
        _M_IDLY_RD(0xB7, _LDA)
    /* CLV */
        _M_IMPLIED(0xB8, c->P&=(uint8_t)~W65C816_VF;)
    /* LDA abs,Y */
        _M_ABXY_RD(0xB9, _LDA, c->Y)
    /* TSX */
        _M_IMPLIED(0xBA, _TR_TSX();)
    /* TYX */
        _M_IMPLIED(0xBB, _TR_TYX();)
    /* LDY abs,X */
        _M_ABXY_RDX(0xBC, _LDY, c->X)
    /* LDA abs,X */
        _M_ABXY_RD(0xBD, _LDA, c->X)
    /* LDX abs,Y */
        _M_ABXY_RDX(0xBE, _LDX, c->Y)
    /* LDA al,X */
        _M_ABLX_RD(0xBF, _LDA)
    /* CPY # */
        _M_IMM_X(0xC0, _CPY)
    /* CMP (dp,X) */
        _M_IDX_RD(0xC1, _CMPA)
    /* REP # */
        _M_IMM_FLAGS(0xC2, 0)
    /* CMP sr,S */
        _M_SR_RD(0xC3, _CMPA)
    /* CPY dp */
        _M_DP_RDX(0xC4, _CPY)
    /* CMP dp */
        _M_DP_RD(0xC5, _CMPA)
    /* DEC dp */
        _M_DP_RMW(0xC6, _RMW_DEC)
    /* CMP [dp] */
        _M_IDL_RD(0xC7, _CMPA)
    /* INY */
        _M_IMPLIED(0xC8, _INY();)
    /* CMP # */
        _M_IMM_RD(0xC9, _CMPA)
    /* DEX */
        _M_IMPLIED(0xCA, _DEX();)
    /* WAI: 3 cycles, then the CPU halts until RES/NMI/ABORT/IRQ */
        case (0xCB<<4)|0: _DUMMY(); break; \
        case (0xCB<<4)|1: _DUMMY(); break; \
        case (0xCB<<4)|2: c->halted=1; break;
    /* CPY abs */
        _M_ABS_RDX(0xCC, _CPY)
    /* CMP abs */
        _M_ABS_RD(0xCD, _CMPA)
    /* DEC abs */
        _M_ABS_RMW(0xCE, _RMW_DEC)
    /* CMP al */
        _M_ABL_RD(0xCF, _CMPA)
    /* BNE */
        _M_BRANCH(0xD0, !(c->P&W65C816_ZF))
    /* CMP (dp),Y */
        _M_IDY_RD(0xD1, _CMPA)
    /* CMP (dp) */
        _M_DPIND_RD(0xD2, _CMPA)
    /* CMP (sr,S),Y */
        _M_SRIY_RD(0xD3, _CMPA)
    /* PEI */
        _M_PEI(0xD4)
    /* CMP dp,X */
        _M_DPXY_RD(0xD5, _CMPA, c->X)
    /* DEC dp,X */
        _M_DPXY_RMW(0xD6, _RMW_DEC, c->X)
    /* CMP [dp],Y */
        _M_IDLY_RD(0xD7, _CMPA)
    /* CLD */
        _M_IMPLIED(0xD8, c->P&=(uint8_t)~W65C816_DF;)
    /* CMP abs,Y */
        _M_ABXY_RD(0xD9, _CMPA, c->Y)
    /* PHX */
        _M_PUSH_W(0xDA, c->X, _X8())
    /* STP: 3 cycles, then the CPU halts until RES */
        case (0xDB<<4)|0: _DUMMY(); break; \
        case (0xDB<<4)|1: _DUMMY(); break; \
        case (0xDB<<4)|2: c->halted=2; break;
    /* JML (al) */
        _M_JML_IND(0xDC)
    /* CMP abs,X */
        _M_ABXY_RD(0xDD, _CMPA, c->X)
    /* DEC abs,X */
        _M_ABXY_RMW(0xDE, _RMW_DEC, c->X)
    /* CMP al,X */
        _M_ABLX_RD(0xDF, _CMPA)
    /* CPX # */
        _M_IMM_X(0xE0, _CPX)
    /* SBC (dp,X) */
        _M_IDX_RD(0xE1, _SBC)
    /* SEP # */
        _M_IMM_FLAGS(0xE2, 1)
    /* SBC sr,S */
        _M_SR_RD(0xE3, _SBC)
    /* CPX dp */
        _M_DP_RDX(0xE4, _CPX)
    /* SBC dp */
        _M_DP_RD(0xE5, _SBC)
    /* INC dp */
        _M_DP_RMW(0xE6, _RMW_INC)
    /* SBC [dp] */
        _M_IDL_RD(0xE7, _SBC)
    /* INX */
        _M_IMPLIED(0xE8, _INX();)
    /* SBC # */
        _M_IMM_RD(0xE9, _SBC)
    /* NOP */
        _M_IMPLIED(0xEA, ;)
    /* XBA (3 cycles) */
        case (0xEB<<4)|0: _DUMMY(); break; \
        case (0xEB<<4)|1: _TR_XBA(); _DUMMY(); break; \
        case (0xEB<<4)|2: _FETCH(); break;
    /* CPX abs */
        _M_ABS_RDX(0xEC, _CPX)
    /* SBC abs */
        _M_ABS_RD(0xED, _SBC)
    /* INC abs */
        _M_ABS_RMW(0xEE, _RMW_INC)
    /* SBC al */
        _M_ABL_RD(0xEF, _SBC)
    /* BEQ */
        _M_BRANCH(0xF0, (c->P&W65C816_ZF))
    /* SBC (dp),Y */
        _M_IDY_RD(0xF1, _SBC)
    /* SBC (dp) */
        _M_DPIND_RD(0xF2, _SBC)
    /* SBC (sr,S),Y */
        _M_SRIY_RD(0xF3, _SBC)
    /* PEA */
        _M_PEA(0xF4)
    /* SBC dp,X */
        _M_DPXY_RD(0xF5, _SBC, c->X)
    /* INC dp,X */
        _M_DPXY_RMW(0xF6, _RMW_INC, c->X)
    /* SBC [dp],Y */
        _M_IDLY_RD(0xF7, _SBC)
    /* SED */
        _M_IMPLIED(0xF8, c->P|=(uint8_t)W65C816_DF;)
    /* SBC abs,Y */
        _M_ABXY_RD(0xF9, _SBC, c->Y)
    /* PLX */
        _M_PULL_W(0xFA, _PLX8, _PLX16, _X8())
    /* XCE (the C<->E swap becomes visible on the E pin with the next bus
       cycle, so the internal cycle still shows the old E state) */
        case (0xFB<<4)|0: _DUMMY(); break; \
        case (0xFB<<4)|1: { uint8_t oe=c->E; _TR_XCE_PREP(); if (oe) { c->P|=(uint8_t)W65C816_CF; } else { c->P&=(uint8_t)~W65C816_CF; } _FETCH(); } break;
    /* JSR (abs,X) */
        _M_JSR_INDX(0xFC)
    /* SBC abs,X */
        _M_ABXY_RD(0xFD, _SBC, c->X)
    /* INC abs,X */
        _M_ABXY_RMW(0xFE, _RMW_INC, c->X)
    /* SBC al,X */
        _M_ABLX_RD(0xFF, _SBC)

    default:
        CHIPS_ASSERT(false);
        break;
    }
    _STATUS_PINS();
    c->PINS = pins;
    c->irq_pip <<= 1;
    c->nmi_pip <<= 1;
    c->abrt_pip <<= 1;
    return pins;
}

#undef _SA
#undef _SAD
#undef _SD
#undef _GD
#undef _PB_PC
#undef _ON
#undef _OFF
#undef _RD
#undef _WR
#undef _VPA
#undef _VDA
#undef _VPB
#undef _MLB
#undef _FETCH
#undef _DUMMY
#undef _NZ8
#undef _NZ16
#undef _W8
#undef _X8
#undef _SH
#undef _STATUS_PINS
#undef _W65C816_TODO
#endif /* CHIPS_IMPL */
