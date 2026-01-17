#pragma once
/*#
    # m6502_connomore64.h

    MOS Technology 6502 CPU emulator.

    Work in progress!
    Part of the https://github.com/c1570/Connomore64 project
    Based on https://github.com/floooh/chips/ but modified for speed.

    The original m6502.h is zlib/libpng licensed;
    m6502_connomore64.h is AGPL 3 licensed, i.e., in case you use
    this in any project, you have to make the complete sources
    of that project available under the AGPL.

    Copyright (c) 2022-2026 https://github.com/c1570
    https://www.gnu.org/licenses/agpl-3.0.html

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

#ifdef __cplusplus
extern "C" {
#endif

#define HAVE_CONNOMORE_M6502H

// control pins
#define M6502_PIN_RW    (0)      // out: memory read or write access
#define M6502_PIN_SYNC  (1)      // out: start of a new instruction
#define M6502_PIN_IRQ   (2)      // in: maskable interrupt requested
#define M6502_PIN_NMI   (3)      // in: non-maskable interrupt requested
#define M6502_PIN_RDY   (4)      // in: freeze execution at next read cycle
#define M6510_PIN_AEC   (5)      // in, m6510 only, put bus lines into tristate mode, not implemented
#define M6502_PIN_RES   (6)      // request RESET

// pin bit masks
#define M6502_RW    (1UL<<M6502_PIN_RW)
#define M6502_SYNC  (1UL<<M6502_PIN_SYNC)
#define M6502_IRQ   (1UL<<M6502_PIN_IRQ)
#define M6502_NMI   (1UL<<M6502_PIN_NMI)
#define M6502_RDY   (1UL<<M6502_PIN_RDY)
#define M6510_AEC   (1UL<<M6510_PIN_AEC)
#define M6502_RES   (1UL<<M6502_PIN_RES)

/* bit mask for all CPU pins (up to bit pos 25) */
#define M6502_PIN_MASK ((1UL<<25)-1)

/* status indicator flags */
#define M6502_CF    (1<<0)  /* carry */
#define M6502_ZF    (1<<1)  /* zero */
#define M6502_IF    (1<<2)  /* IRQ disable */
#define M6502_DF    (1<<3)  /* decimal mode */
#define M6502_BF    (1<<4)  /* BRK command */
#define M6502_XF    (1<<5)  /* unused */
#define M6502_VF    (1<<6)  /* overflow */
#define M6502_NF    (1<<7)  /* negative */

/* internal BRK state flags */
#define M6502_BRK_IRQ   (1<<0)  /* IRQ was triggered */
#define M6502_BRK_NMI   (1<<1)  /* NMI was triggered */
#define M6502_BRK_RESET (1<<2)  /* RES was triggered */

/* m6510 IO port callback prototypes */
typedef void (*m6510_out_t)(uint8_t data, void* user_data);
typedef uint8_t (*m6510_in_t)(void* user_data);

/* the desc structure provided to m6502_init() */
typedef struct {
    bool bcd_disabled;              /* set to true if BCD mode is disabled */
    m6510_in_t m6510_in_cb;         /* optional port IO input callback (only on m6510) */
    m6510_out_t m6510_out_cb;       /* optional port IO output callback (only on m6510) */
    void* m6510_user_data;          /* optional callback user data */
    uint8_t m6510_io_pullup;        /* IO port bits that are 1 when reading */
    uint8_t m6510_io_floating;      /* unconnected IO port pins */
} m6502_desc_t;

/* CPU state */
typedef struct {
    uint16_t IR;        /* internal instruction register */
    uint16_t PC;        /* internal program counter register */
    uint16_t AD;        /* ADL/ADH internal register */
    uint8_t A,X,Y,S,P;  /* regular registers */
    uint32_t PINS;      /* last stored pin state (do NOT modify) */
    uint32_t int_pip;   /* combined nmi (upper 16 bits) and irq (lower 16 bits) pipeline */
    uint8_t brk_flags;  /* M6502_BRK_* */
    uint8_t bcd_enabled;
    /* 6510 IO port state */
    void* user_data;
    m6510_in_t in_cb;
    m6510_out_t out_cb;
    uint8_t io_ddr;     /* 1: output, 0: input */
    uint8_t io_inp;     /* last port input */
    uint8_t io_out;     /* last port output */
    uint8_t io_pullup;
    uint8_t io_floating;
    uint8_t io_drive;

    uint8_t io_pins;    /* current state of IO pins (combined input/output) */
    uint16_t bus_addr;
    uint8_t bus_data;
} m6502_t;

/* initialize a new m6502 instance and return initial pin mask */
uint32_t m6502_init(m6502_t* cpu, const m6502_desc_t* desc);
/* execute one tick */
uint32_t m6502_tick(m6502_t* cpu, uint32_t pins);
/* perform m6510 port IO (only call this if M6510_CHECK_IO(pins) is true) */
uint32_t m6510_iorq(m6502_t* cpu, uint32_t pins);
// prepare m6502_t snapshot for saving
void m6502_snapshot_onsave(m6502_t* snapshot);
// fixup m6502_t snapshot after loading
void m6502_snapshot_onload(m6502_t* snapshot, m6502_t* sys);

/* register access functions */
void m6502_set_a(m6502_t* cpu, uint8_t v);
void m6502_set_x(m6502_t* cpu, uint8_t v);
void m6502_set_y(m6502_t* cpu, uint8_t v);
void m6502_set_s(m6502_t* cpu, uint8_t v);
void m6502_set_p(m6502_t* cpu, uint8_t v);
void m6502_set_pc(m6502_t* cpu, uint16_t v);
uint8_t m6502_a(m6502_t* cpu);
uint8_t m6502_x(m6502_t* cpu);
uint8_t m6502_y(m6502_t* cpu);
uint8_t m6502_s(m6502_t* cpu);
uint8_t m6502_p(m6502_t* cpu);
uint16_t m6502_pc(m6502_t* cpu);

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
void m6502_set_a(m6502_t* cpu, uint8_t v) { cpu->A = v; }
void m6502_set_x(m6502_t* cpu, uint8_t v) { cpu->X = v; }
void m6502_set_y(m6502_t* cpu, uint8_t v) { cpu->Y = v; }
void m6502_set_s(m6502_t* cpu, uint8_t v) { cpu->S = v; }
void m6502_set_p(m6502_t* cpu, uint8_t v) { cpu->P = v; }
void m6502_set_pc(m6502_t* cpu, uint16_t v) { cpu->PC = v; }
uint8_t m6502_a(m6502_t* cpu) { return cpu->A; }
uint8_t m6502_x(m6502_t* cpu) { return cpu->X; }
uint8_t m6502_y(m6502_t* cpu) { return cpu->Y; }
uint8_t m6502_s(m6502_t* cpu) { return cpu->S; }
uint8_t m6502_p(m6502_t* cpu) { return cpu->P; }
uint16_t m6502_pc(m6502_t* cpu) { return cpu->PC; }

/* helper macros and functions for code-generated instruction decoder */
#define _M6502_NZ(p,v) ((p&~(M6502_NF|M6502_ZF))|((v&0xFF)?(v&M6502_NF):M6502_ZF))

static inline void __attribute__((always_inline)) _m6502_adc(m6502_t* cpu, uint8_t val) {
    if (/*cpu->bcd_enabled && */(cpu->P & M6502_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & M6502_CF ? 1 : 0;
        cpu->P &= ~(M6502_NF|M6502_VF|M6502_ZF|M6502_CF);
        uint8_t al = (cpu->A & 0x0F) + (val & 0x0F) + c;
        if (al > 9) {
            al += 6;
        }
        uint8_t ah = (cpu->A >> 4) + (val >> 4) + (al > 0x0F);
        if (0 == (uint8_t)(cpu->A + val + c)) {
            cpu->P |= M6502_ZF;
        }
        else if (ah & 0x08) {
            cpu->P |= M6502_NF;
        }
        if (~(cpu->A^val) & (cpu->A^(ah<<4)) & 0x80) {
            cpu->P |= M6502_VF;
        }
        if (ah > 9) {
            ah += 6;
        }
        if (ah > 15) {
            cpu->P |= M6502_CF;
        }
        cpu->A = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        register uint16_t sum = cpu->A + val + (cpu->P & M6502_CF ? 1:0);
        register uint8_t P = cpu->P;
        P &= ~(M6502_VF|M6502_CF);
        P = _M6502_NZ(P,sum);
        if(likely(~(cpu->A^val) & (cpu->A^sum) & 0x80)) {
            P |= M6502_VF;
        }
        if(likely(sum & 0xFF00)) {
            P |= M6502_CF;
        }
        cpu->A = sum & 0xFF;
        cpu->P = P;
    }
}

static inline void __attribute__((always_inline)) _m6502_sbc(m6502_t* cpu, uint8_t val) {
    if (/*cpu->bcd_enabled && */(cpu->P & M6502_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & M6502_CF ? 0 : 1;
        cpu->P &= ~(M6502_NF|M6502_VF|M6502_ZF|M6502_CF);
        uint16_t diff = cpu->A - val - c;
        uint8_t al = (cpu->A & 0x0F) - (val & 0x0F) - c;
        if ((int8_t)al < 0) {
            al -= 6;
        }
        uint8_t ah = (cpu->A>>4) - (val>>4) - ((int8_t)al < 0);
        if (0 == (uint8_t)diff) {
            cpu->P |= M6502_ZF;
        }
        else if (diff & 0x80) {
            cpu->P |= M6502_NF;
        }
        if ((cpu->A^val) & (cpu->A^diff) & 0x80) {
            cpu->P |= M6502_VF;
        }
        if (!(diff & 0xFF00)) {
            cpu->P |= M6502_CF;
        }
        if (ah & 0x80) {
            ah -= 6;
        }
        cpu->A = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        register uint16_t diff = cpu->A - val - (cpu->P & M6502_CF ? 0 : 1);
        register uint8_t P = cpu->P;
        P &= ~(M6502_VF|M6502_CF);
        P = _M6502_NZ(P, (uint8_t)diff);
        if(likely((cpu->A^val) & (cpu->A^diff) & 0x80)) {
            P |= M6502_VF;
        }
        if(likely(!(diff & 0xFF00))) {
            P |= M6502_CF;
        }
        cpu->A = diff & 0xFF;
        cpu->P = P;
    }
}

static inline void _m6502_cmp(m6502_t* cpu, uint8_t r, uint8_t v) {
    uint16_t t = r - v;
    cpu->P = (_M6502_NZ(cpu->P, (uint8_t)t) & ~M6502_CF) | ((t & 0xFF00) ? 0:M6502_CF);
}

static inline uint8_t _m6502_asl(m6502_t* cpu, uint8_t v) {
    cpu->P = (_M6502_NZ(cpu->P, v<<1) & ~M6502_CF) | ((v & 0x80) ? M6502_CF:0);
    return v<<1;
}

static inline uint8_t _m6502_lsr(m6502_t* cpu, uint8_t v) {
    cpu->P = (_M6502_NZ(cpu->P, v>>1) & ~M6502_CF) | ((v & 0x01) ? M6502_CF:0);
    return v>>1;
}

static inline uint8_t __attribute__((always_inline)) _m6502_rol(m6502_t* cpu, uint8_t v) {
    bool carry = cpu->P & M6502_CF;
    cpu->P &= ~(M6502_NF|M6502_ZF|M6502_CF);
    if (v & 0x80) {
        cpu->P |= M6502_CF;
    }
    v <<= 1;
    if (carry) {
        v |= 1;
    }
    cpu->P = _M6502_NZ(cpu->P, v);
    return v;
}

static inline uint8_t __attribute__((always_inline)) _m6502_ror(m6502_t* cpu, uint8_t v) {
    bool carry = cpu->P & M6502_CF;
    cpu->P &= ~(M6502_NF|M6502_ZF|M6502_CF);
    if (v & 1) {
        cpu->P |= M6502_CF;
    }
    v >>= 1;
    if (carry) {
        v |= 0x80;
    }
    cpu->P = _M6502_NZ(cpu->P, v);
    return v;
}

static inline void __attribute__((always_inline)) _m6502_bit(m6502_t* cpu, uint8_t v) {
    uint8_t t = cpu->A & v;
    cpu->P &= ~(M6502_NF|M6502_VF|M6502_ZF);
    if (!t) {
        cpu->P |= M6502_ZF;
    }
    cpu->P |= v & (M6502_NF|M6502_VF);
}

static inline void __attribute__((always_inline)) _m6502_arr(m6502_t* cpu) {
    /* undocumented, unreliable ARR instruction, but this is tested
       by the Wolfgang Lorenz C64 test suite
       implementation taken from MAME
    */
    if (/*cpu->bcd_enabled && */(cpu->P & M6502_DF)) {
        bool c = cpu->P & M6502_CF;
        cpu->P &= ~(M6502_NF|M6502_VF|M6502_ZF|M6502_CF);
        uint8_t a = cpu->A>>1;
        if (c) {
            a |= 0x80;
        }
        cpu->P = _M6502_NZ(cpu->P,a);
        if ((a ^ cpu->A) & 0x40) {
            cpu->P |= M6502_VF;
        }
        if ((cpu->A & 0xF) >= 5) {
            a = ((a + 6) & 0xF) | (a & 0xF0);
        }
        if ((cpu->A & 0xF0) >= 0x50) {
            a += 0x60;
            cpu->P |= M6502_CF;
        }
        cpu->A = a;
    }
    else {
        bool c = cpu->P & M6502_CF;
        cpu->P &= ~(M6502_NF|M6502_VF|M6502_ZF|M6502_CF);
        cpu->A >>= 1;
        if (c) {
            cpu->A |= 0x80;
        }
        cpu->P = _M6502_NZ(cpu->P,cpu->A);
        if (cpu->A & 0x40) {
            cpu->P |= M6502_VF|M6502_CF;
        }
        if (cpu->A & 0x20) {
            cpu->P ^= M6502_VF;
        }
    }
}

/* undocumented SBX instruction:
    AND X register with accumulator and store result in X register, then
    subtract byte from X register (without borrow) where the
    subtract works like a CMP instruction
*/
static inline void __attribute__((always_inline)) _m6502_sbx(m6502_t* cpu, uint8_t v) {
    uint16_t t = (cpu->A & cpu->X) - v;
    cpu->P = _M6502_NZ(cpu->P, t) & ~M6502_CF;
    if (!(t & 0xFF00)) {
        cpu->P |= M6502_CF;
    }
    cpu->X = (uint8_t)t;
}
#undef _M6502_NZ

uint32_t m6502_init(m6502_t* c, const m6502_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    memset(c, 0, sizeof(*c));
    c->P = M6502_ZF;
    c->bcd_enabled = !desc->bcd_disabled;
    c->PINS = M6502_RW | M6502_SYNC | M6502_RES;
    c->in_cb = desc->m6510_in_cb;
    c->out_cb = desc->m6510_out_cb;
    c->user_data = desc->m6510_user_data;
    c->io_pullup = desc->m6510_io_pullup;
    c->io_floating = desc->m6510_io_floating;
    return c->PINS;
}

/* only call this when accessing address 0 or 1 (M6510_CHECK_IO(pins) evaluates to true) */
uint32_t m6510_iorq(m6502_t* c, uint32_t pins) {
    CHIPS_ASSERT(c->in_cb && c->out_cb);
    if ((c->bus_addr & 1) == 0) {
        /* address 0: access to data direction register */
        if (pins & M6502_RW) {
            /* read IO direction bits */
            c->bus_data = c->io_ddr;
        }
        else {
            /* write IO direction bits and update outside world */
            c->io_ddr = c->bus_data;
            c->io_drive = (c->io_out & c->io_ddr) | (c->io_drive & ~c->io_ddr);
            c->out_cb((c->io_out & c->io_ddr) | (c->io_pullup & ~c->io_ddr), c->user_data);
            c->io_pins = (c->io_out & c->io_ddr) | (c->io_inp & ~c->io_ddr);
        }
    }
    else {
        /* address 1: perform I/O */
        if (pins & M6502_RW) {
            /* an input operation */
            c->io_inp = c->in_cb(c->user_data);
            uint8_t val = ((c->io_inp | (c->io_floating & c->io_drive)) & ~c->io_ddr) | (c->io_out & c->io_ddr);
            c->bus_data = val;
        }
        else {
            /* an output operation */
            c->io_out = c->bus_data;
            c->io_drive = (c->io_out & c->io_ddr) | (c->io_drive & ~c->io_ddr);
            c->out_cb((c->io_out & c->io_ddr) | (c->io_pullup & ~c->io_ddr), c->user_data);
        }
        c->io_pins = (c->io_out & c->io_ddr) | (c->io_inp & ~c->io_ddr);
    }
    return pins;
}

void m6502_snapshot_onsave(m6502_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    snapshot->in_cb = 0;
    snapshot->out_cb = 0;
    snapshot->user_data = 0;
}

void m6502_snapshot_onload(m6502_t* snapshot, m6502_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    snapshot->in_cb = sys->in_cb;
    snapshot->out_cb = sys->out_cb;
    snapshot->user_data = sys->user_data;
}

/* set 16-bit address */
#define _SA(addr) c->bus_addr=addr
/* get 16-bit address */
#define _GA() c->bus_addr
/* set 16-bit address and 8-bit data */
#define _SAD(addr,data) {c->bus_addr=addr;c->bus_data=data;}
/* fetch next opcode byte */
#define _FETCH() _SA(c->PC);_ON(M6502_SYNC);
/* set 8-bit data */
#define _SD(data) c->bus_data=data
/* get 8-bit bus data */
#define _GD() (c->bus_data)
/* enable control pins */
#define _ON(m) pins|=(m)
/* disable control pins */
#define _OFF(m) pins&=~(m)
/* a memory read tick */
#define _RD() _ON(M6502_RW);
/* a memory write tick */
#define _WR() _OFF(M6502_RW);
/* set N and Z flags depending on value */
#define _NZ(v) c->P=((c->P&~(M6502_NF|M6502_ZF))|((v&0xFF)?(v&M6502_NF):M6502_ZF))
/* ignore data */
#define _ID()
/* ignore address */
#define _IA()

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4244)   /* conversion from 'uint16_t' to 'uint8_t', possible loss of data */
#endif

uint32_t m6502_tick(m6502_t* c, uint32_t pins) {
    if (pins & (M6502_SYNC|M6502_IRQ|M6502_NMI|M6502_RDY|M6502_RES)) {
        // interrupt detection also works in RDY phases, but only NMI is "sticky"

        // NMI is edge-triggered
        if (0 != ((pins & (pins ^ c->PINS)) & M6502_NMI)) {
            c->int_pip |= 0x00100000;
        }
        // IRQ test is level triggered
        if ((pins & M6502_IRQ) && (0 == (c->P & M6502_IF))) {
            c->int_pip |= 0x00000100;
        }

        // RDY pin is only checked during read cycles
        if ((pins & (M6502_RW|M6502_RDY)) == (M6502_RW|M6502_RDY)) {
            c->PINS = pins;
            c->int_pip = (c->int_pip & 0xffff0000) | ((c->int_pip & 0x7fff) << 1);
            _IA();
            _ID();
            return pins;
        }
        if (pins & M6502_SYNC) {
            _OFF(M6502_SYNC);
            // check IRQ, NMI and RES state
            //  - IRQ is level-triggered and must be active in the full cycle
            //    before SYNC
            //  - NMI is edge-triggered, and the change must have happened in
            //    any cycle before SYNC
            //  - RES behaves slightly different than on a real 6502, we go
            //    into RES state as soon as the pin goes active, from there
            //    on, behaviour is 'standard'
            if (0 != (c->int_pip & 0x400)) {
                c->brk_flags |= M6502_BRK_IRQ;
            }
            if (0 != (c->int_pip & 0xFFC00000)) {
                c->brk_flags |= M6502_BRK_NMI;
            }
            if (0 != (pins & M6502_RES)) {
                c->brk_flags |= M6502_BRK_RESET;
                c->io_ddr = 0;
                c->io_out = 0;
                c->io_inp = 7;
                c->io_pins = 7;
            }
            c->int_pip &= 0x003F03FF;
            c->int_pip <<= 1;

            // if interrupt or reset was requested, force a BRK instruction
            // otherwise, load new instruction into 'instruction register' and restart tick counter
            if (c->brk_flags) {
                c->IR = 0;
                c->P &= ~M6502_BF;
                pins &= ~M6502_RES;
                _ID();
            }
            else {
                c->PC++;
                c->IR = _GD()<<3;
            }
        } else {
            c->int_pip &= 0xffff7fff;
            c->int_pip <<= 1;
        }
    } else {
        c->int_pip &= 0xffff7fff;
        c->int_pip <<= 1;
    }
    // reads are default, writes are special
    _RD();
    switch (c->IR++) {
    /* BRK  */
        case (0x00<<3)|0: _SA(c->PC);break;
        case (0x00<<3)|1: if(0==(c->brk_flags&(M6502_BRK_IRQ|M6502_BRK_NMI))){c->PC++;}_SAD(0x0100|c->S--,c->PC>>8);if(0==(c->brk_flags&M6502_BRK_RESET)){_WR();}_ID();break;
        case (0x00<<3)|2: _SAD(0x0100|c->S--,c->PC);if(0==(c->brk_flags&M6502_BRK_RESET)){_WR();}_ID();break;
        case (0x00<<3)|3: _SAD(0x0100|c->S--,c->P|M6502_XF);if(c->brk_flags&M6502_BRK_RESET){c->AD=0xFFFC;}else{_WR();if(c->brk_flags&M6502_BRK_NMI){c->AD=0xFFFA;}else{c->AD=0xFFFE;}}_ID();break;
        case (0x00<<3)|4: _SA(c->AD++);c->P|=(M6502_IF|M6502_BF);c->brk_flags=0; /* RES/NMI hijacking */_ID();break;
        case (0x00<<3)|5: _SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */break;
        case (0x00<<3)|6: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x00<<3)|7: assert(false);break;
    /* ORA (zp,X) */
        case (0x01<<3)|0: _SA(c->PC++);break;
        case (0x01<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x01<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x01<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x01<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x01<<3)|5: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x01<<3)|6: assert(false);break;
        case (0x01<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x02<<3)|0: _SA(c->PC);break;
        case (0x02<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x02<<3)|2: assert(false);break;
        case (0x02<<3)|3: assert(false);break;
        case (0x02<<3)|4: assert(false);break;
        case (0x02<<3)|5: assert(false);break;
        case (0x02<<3)|6: assert(false);break;
        case (0x02<<3)|7: assert(false);break;
    /* SLO (zp,X) (undoc) */
        case (0x03<<3)|0: _SA(c->PC++);break;
        case (0x03<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x03<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x03<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x03<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x03<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x03<<3)|6: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x03<<3)|7: _FETCH();_ID();break;
    /* NOP zp (undoc) */
        case (0x04<<3)|0: _SA(c->PC++);break;
        case (0x04<<3)|1: _SA(_GD());break;
        case (0x04<<3)|2: _FETCH();_ID();break;
        case (0x04<<3)|3: assert(false);break;
        case (0x04<<3)|4: assert(false);break;
        case (0x04<<3)|5: assert(false);break;
        case (0x04<<3)|6: assert(false);break;
        case (0x04<<3)|7: assert(false);break;
    /* ORA zp */
        case (0x05<<3)|0: _SA(c->PC++);break;
        case (0x05<<3)|1: _SA(_GD());break;
        case (0x05<<3)|2: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x05<<3)|3: assert(false);break;
        case (0x05<<3)|4: assert(false);break;
        case (0x05<<3)|5: assert(false);break;
        case (0x05<<3)|6: assert(false);break;
        case (0x05<<3)|7: assert(false);break;
    /* ASL zp */
        case (0x06<<3)|0: _SA(c->PC++);break;
        case (0x06<<3)|1: _SA(_GD());break;
        case (0x06<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x06<<3)|3: _WR();_SD(_m6502_asl(c,c->AD));_ID();break;
        case (0x06<<3)|4: _FETCH();_ID();break;
        case (0x06<<3)|5: assert(false);break;
        case (0x06<<3)|6: assert(false);break;
        case (0x06<<3)|7: assert(false);break;
    /* SLO zp (undoc) */
        case (0x07<<3)|0: _SA(c->PC++);break;
        case (0x07<<3)|1: _SA(_GD());break;
        case (0x07<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x07<<3)|3: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x07<<3)|4: _FETCH();_ID();break;
        case (0x07<<3)|5: assert(false);break;
        case (0x07<<3)|6: assert(false);break;
        case (0x07<<3)|7: assert(false);break;
    /* PHP  */
        case (0x08<<3)|0: _SA(c->PC);break;
        case (0x08<<3)|1: _WR();_SAD(0x0100|c->S--,c->P|M6502_XF);_ID();break;
        case (0x08<<3)|2: _FETCH();_ID();break;
        case (0x08<<3)|3: assert(false);break;
        case (0x08<<3)|4: assert(false);break;
        case (0x08<<3)|5: assert(false);break;
        case (0x08<<3)|6: assert(false);break;
        case (0x08<<3)|7: assert(false);break;
    /* ORA # */
        case (0x09<<3)|0: _SA(c->PC++);break;
        case (0x09<<3)|1: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x09<<3)|2: assert(false);break;
        case (0x09<<3)|3: assert(false);break;
        case (0x09<<3)|4: assert(false);break;
        case (0x09<<3)|5: assert(false);break;
        case (0x09<<3)|6: assert(false);break;
        case (0x09<<3)|7: assert(false);break;
    /* ASLA  */
        case (0x0A<<3)|0: _SA(c->PC);break;
        case (0x0A<<3)|1: _FETCH();c->A=_m6502_asl(c,c->A);_ID();break;
        case (0x0A<<3)|2: assert(false);break;
        case (0x0A<<3)|3: assert(false);break;
        case (0x0A<<3)|4: assert(false);break;
        case (0x0A<<3)|5: assert(false);break;
        case (0x0A<<3)|6: assert(false);break;
        case (0x0A<<3)|7: assert(false);break;
    /* ANC # (undoc) */
        case (0x0B<<3)|0: _SA(c->PC++);break;
        case (0x0B<<3)|1: _FETCH();c->A&=_GD();_NZ(c->A);if(c->A&0x80){c->P|=M6502_CF;}else{c->P&=~M6502_CF;}break;
        case (0x0B<<3)|2: assert(false);break;
        case (0x0B<<3)|3: assert(false);break;
        case (0x0B<<3)|4: assert(false);break;
        case (0x0B<<3)|5: assert(false);break;
        case (0x0B<<3)|6: assert(false);break;
        case (0x0B<<3)|7: assert(false);break;
    /* NOP abs (undoc) */
        case (0x0C<<3)|0: _SA(c->PC++);break;
        case (0x0C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x0C<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0C<<3)|3: _FETCH();_ID();break;
        case (0x0C<<3)|4: assert(false);break;
        case (0x0C<<3)|5: assert(false);break;
        case (0x0C<<3)|6: assert(false);break;
        case (0x0C<<3)|7: assert(false);break;
    /* ORA abs */
        case (0x0D<<3)|0: _SA(c->PC++);break;
        case (0x0D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x0D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0D<<3)|3: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x0D<<3)|4: assert(false);break;
        case (0x0D<<3)|5: assert(false);break;
        case (0x0D<<3)|6: assert(false);break;
        case (0x0D<<3)|7: assert(false);break;
    /* ASL abs */
        case (0x0E<<3)|0: _SA(c->PC++);break;
        case (0x0E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x0E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0E<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x0E<<3)|4: _WR();_SD(_m6502_asl(c,c->AD));_ID();break;
        case (0x0E<<3)|5: _FETCH();_ID();break;
        case (0x0E<<3)|6: assert(false);break;
        case (0x0E<<3)|7: assert(false);break;
    /* SLO abs (undoc) */
        case (0x0F<<3)|0: _SA(c->PC++);break;
        case (0x0F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x0F<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0F<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x0F<<3)|4: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x0F<<3)|5: _FETCH();_ID();break;
        case (0x0F<<3)|6: assert(false);break;
        case (0x0F<<3)|7: assert(false);break;
    /* BPL # */
        case (0x10<<3)|0: _SA(c->PC++);break;
        case (0x10<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x0){_FETCH();}else{_SA(c->PC);};break;
        case (0x10<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0x10<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0x10<<3)|4: assert(false);break;
        case (0x10<<3)|5: assert(false);break;
        case (0x10<<3)|6: assert(false);break;
        case (0x10<<3)|7: assert(false);break;
    /* ORA (zp),Y */
        case (0x11<<3)|0: _SA(c->PC++);break;
        case (0x11<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x11<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x11<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x11<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x11<<3)|5: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x11<<3)|6: assert(false);break;
        case (0x11<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x12<<3)|0: _SA(c->PC);break;
        case (0x12<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x12<<3)|2: assert(false);break;
        case (0x12<<3)|3: assert(false);break;
        case (0x12<<3)|4: assert(false);break;
        case (0x12<<3)|5: assert(false);break;
        case (0x12<<3)|6: assert(false);break;
        case (0x12<<3)|7: assert(false);break;
    /* SLO (zp),Y (undoc) */
        case (0x13<<3)|0: _SA(c->PC++);break;
        case (0x13<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x13<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x13<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x13<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x13<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x13<<3)|6: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x13<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0x14<<3)|0: _SA(c->PC++);break;
        case (0x14<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x14<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x14<<3)|3: _FETCH();_ID();break;
        case (0x14<<3)|4: assert(false);break;
        case (0x14<<3)|5: assert(false);break;
        case (0x14<<3)|6: assert(false);break;
        case (0x14<<3)|7: assert(false);break;
    /* ORA zp,X */
        case (0x15<<3)|0: _SA(c->PC++);break;
        case (0x15<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x15<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x15<<3)|3: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x15<<3)|4: assert(false);break;
        case (0x15<<3)|5: assert(false);break;
        case (0x15<<3)|6: assert(false);break;
        case (0x15<<3)|7: assert(false);break;
    /* ASL zp,X */
        case (0x16<<3)|0: _SA(c->PC++);break;
        case (0x16<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x16<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x16<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x16<<3)|4: _WR();_SD(_m6502_asl(c,c->AD));_ID();break;
        case (0x16<<3)|5: _FETCH();_ID();break;
        case (0x16<<3)|6: assert(false);break;
        case (0x16<<3)|7: assert(false);break;
    /* SLO zp,X (undoc) */
        case (0x17<<3)|0: _SA(c->PC++);break;
        case (0x17<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x17<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x17<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x17<<3)|4: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x17<<3)|5: _FETCH();_ID();break;
        case (0x17<<3)|6: assert(false);break;
        case (0x17<<3)|7: assert(false);break;
    /* CLC  */
        case (0x18<<3)|0: _SA(c->PC);break;
        case (0x18<<3)|1: _FETCH();c->P&=~0x1;_ID();break;
        case (0x18<<3)|2: assert(false);break;
        case (0x18<<3)|3: assert(false);break;
        case (0x18<<3)|4: assert(false);break;
        case (0x18<<3)|5: assert(false);break;
        case (0x18<<3)|6: assert(false);break;
        case (0x18<<3)|7: assert(false);break;
    /* ORA abs,Y */
        case (0x19<<3)|0: _SA(c->PC++);break;
        case (0x19<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x19<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x19<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x19<<3)|4: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x19<<3)|5: assert(false);break;
        case (0x19<<3)|6: assert(false);break;
        case (0x19<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0x1A<<3)|0: _SA(c->PC);break;
        case (0x1A<<3)|1: _FETCH();_ID();break;
        case (0x1A<<3)|2: assert(false);break;
        case (0x1A<<3)|3: assert(false);break;
        case (0x1A<<3)|4: assert(false);break;
        case (0x1A<<3)|5: assert(false);break;
        case (0x1A<<3)|6: assert(false);break;
        case (0x1A<<3)|7: assert(false);break;
    /* SLO abs,Y (undoc) */
        case (0x1B<<3)|0: _SA(c->PC++);break;
        case (0x1B<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x1B<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x1B<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x1B<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x1B<<3)|5: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x1B<<3)|6: _FETCH();_ID();break;
        case (0x1B<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0x1C<<3)|0: _SA(c->PC++);break;
        case (0x1C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x1C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x1C<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x1C<<3)|4: _FETCH();_ID();break;
        case (0x1C<<3)|5: assert(false);break;
        case (0x1C<<3)|6: assert(false);break;
        case (0x1C<<3)|7: assert(false);break;
    /* ORA abs,X */
        case (0x1D<<3)|0: _SA(c->PC++);break;
        case (0x1D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x1D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x1D<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x1D<<3)|4: _FETCH();c->A|=_GD();_NZ(c->A);break;
        case (0x1D<<3)|5: assert(false);break;
        case (0x1D<<3)|6: assert(false);break;
        case (0x1D<<3)|7: assert(false);break;
    /* ASL abs,X */
        case (0x1E<<3)|0: _SA(c->PC++);break;
        case (0x1E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x1E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x1E<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x1E<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x1E<<3)|5: _WR();_SD(_m6502_asl(c,c->AD));_ID();break;
        case (0x1E<<3)|6: _FETCH();_ID();break;
        case (0x1E<<3)|7: assert(false);break;
    /* SLO abs,X (undoc) */
        case (0x1F<<3)|0: _SA(c->PC++);break;
        case (0x1F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x1F<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x1F<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x1F<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x1F<<3)|5: _WR();c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_ID();break;
        case (0x1F<<3)|6: _FETCH();_ID();break;
        case (0x1F<<3)|7: assert(false);break;
    /* JSR  */
        case (0x20<<3)|0: _SA(c->PC++);break;
        case (0x20<<3)|1: _SA(0x0100|c->S);c->AD=_GD();break;
        case (0x20<<3)|2: _WR();_SAD(0x0100|c->S--,c->PC>>8);_ID();break;
        case (0x20<<3)|3: _WR();_SAD(0x0100|c->S--,c->PC);_ID();break;
        case (0x20<<3)|4: _SA(c->PC);_ID();break;
        case (0x20<<3)|5: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x20<<3)|6: assert(false);break;
        case (0x20<<3)|7: assert(false);break;
    /* AND (zp,X) */
        case (0x21<<3)|0: _SA(c->PC++);break;
        case (0x21<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x21<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x21<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x21<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x21<<3)|5: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x21<<3)|6: assert(false);break;
        case (0x21<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x22<<3)|0: _SA(c->PC);break;
        case (0x22<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x22<<3)|2: assert(false);break;
        case (0x22<<3)|3: assert(false);break;
        case (0x22<<3)|4: assert(false);break;
        case (0x22<<3)|5: assert(false);break;
        case (0x22<<3)|6: assert(false);break;
        case (0x22<<3)|7: assert(false);break;
    /* RLA (zp,X) (undoc) */
        case (0x23<<3)|0: _SA(c->PC++);break;
        case (0x23<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x23<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x23<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x23<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x23<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x23<<3)|6: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x23<<3)|7: _FETCH();_ID();break;
    /* BIT zp */
        case (0x24<<3)|0: _SA(c->PC++);break;
        case (0x24<<3)|1: _SA(_GD());break;
        case (0x24<<3)|2: _FETCH();_m6502_bit(c,_GD());break;
        case (0x24<<3)|3: assert(false);break;
        case (0x24<<3)|4: assert(false);break;
        case (0x24<<3)|5: assert(false);break;
        case (0x24<<3)|6: assert(false);break;
        case (0x24<<3)|7: assert(false);break;
    /* AND zp */
        case (0x25<<3)|0: _SA(c->PC++);break;
        case (0x25<<3)|1: _SA(_GD());break;
        case (0x25<<3)|2: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x25<<3)|3: assert(false);break;
        case (0x25<<3)|4: assert(false);break;
        case (0x25<<3)|5: assert(false);break;
        case (0x25<<3)|6: assert(false);break;
        case (0x25<<3)|7: assert(false);break;
    /* ROL zp */
        case (0x26<<3)|0: _SA(c->PC++);break;
        case (0x26<<3)|1: _SA(_GD());break;
        case (0x26<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x26<<3)|3: _WR();_SD(_m6502_rol(c,c->AD));_ID();break;
        case (0x26<<3)|4: _FETCH();_ID();break;
        case (0x26<<3)|5: assert(false);break;
        case (0x26<<3)|6: assert(false);break;
        case (0x26<<3)|7: assert(false);break;
    /* RLA zp (undoc) */
        case (0x27<<3)|0: _SA(c->PC++);break;
        case (0x27<<3)|1: _SA(_GD());break;
        case (0x27<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x27<<3)|3: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x27<<3)|4: _FETCH();_ID();break;
        case (0x27<<3)|5: assert(false);break;
        case (0x27<<3)|6: assert(false);break;
        case (0x27<<3)|7: assert(false);break;
    /* PLP  */
        case (0x28<<3)|0: _SA(c->PC);break;
        case (0x28<<3)|1: _SA(0x0100|c->S++);_ID();break;
        case (0x28<<3)|2: _SA(0x0100|c->S);_ID();break;
        case (0x28<<3)|3: _FETCH();c->P=(_GD()|M6502_BF)&~M6502_XF;break;
        case (0x28<<3)|4: assert(false);break;
        case (0x28<<3)|5: assert(false);break;
        case (0x28<<3)|6: assert(false);break;
        case (0x28<<3)|7: assert(false);break;
    /* AND # */
        case (0x29<<3)|0: _SA(c->PC++);break;
        case (0x29<<3)|1: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x29<<3)|2: assert(false);break;
        case (0x29<<3)|3: assert(false);break;
        case (0x29<<3)|4: assert(false);break;
        case (0x29<<3)|5: assert(false);break;
        case (0x29<<3)|6: assert(false);break;
        case (0x29<<3)|7: assert(false);break;
    /* ROLA  */
        case (0x2A<<3)|0: _SA(c->PC);break;
        case (0x2A<<3)|1: _FETCH();c->A=_m6502_rol(c,c->A);_ID();break;
        case (0x2A<<3)|2: assert(false);break;
        case (0x2A<<3)|3: assert(false);break;
        case (0x2A<<3)|4: assert(false);break;
        case (0x2A<<3)|5: assert(false);break;
        case (0x2A<<3)|6: assert(false);break;
        case (0x2A<<3)|7: assert(false);break;
    /* ANC # (undoc) */
        case (0x2B<<3)|0: _SA(c->PC++);break;
        case (0x2B<<3)|1: _FETCH();c->A&=_GD();_NZ(c->A);if(c->A&0x80){c->P|=M6502_CF;}else{c->P&=~M6502_CF;}break;
        case (0x2B<<3)|2: assert(false);break;
        case (0x2B<<3)|3: assert(false);break;
        case (0x2B<<3)|4: assert(false);break;
        case (0x2B<<3)|5: assert(false);break;
        case (0x2B<<3)|6: assert(false);break;
        case (0x2B<<3)|7: assert(false);break;
    /* BIT abs */
        case (0x2C<<3)|0: _SA(c->PC++);break;
        case (0x2C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x2C<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2C<<3)|3: _FETCH();_m6502_bit(c,_GD());break;
        case (0x2C<<3)|4: assert(false);break;
        case (0x2C<<3)|5: assert(false);break;
        case (0x2C<<3)|6: assert(false);break;
        case (0x2C<<3)|7: assert(false);break;
    /* AND abs */
        case (0x2D<<3)|0: _SA(c->PC++);break;
        case (0x2D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x2D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2D<<3)|3: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x2D<<3)|4: assert(false);break;
        case (0x2D<<3)|5: assert(false);break;
        case (0x2D<<3)|6: assert(false);break;
        case (0x2D<<3)|7: assert(false);break;
    /* ROL abs */
        case (0x2E<<3)|0: _SA(c->PC++);break;
        case (0x2E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x2E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2E<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x2E<<3)|4: _WR();_SD(_m6502_rol(c,c->AD));_ID();break;
        case (0x2E<<3)|5: _FETCH();_ID();break;
        case (0x2E<<3)|6: assert(false);break;
        case (0x2E<<3)|7: assert(false);break;
    /* RLA abs (undoc) */
        case (0x2F<<3)|0: _SA(c->PC++);break;
        case (0x2F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x2F<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2F<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x2F<<3)|4: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x2F<<3)|5: _FETCH();_ID();break;
        case (0x2F<<3)|6: assert(false);break;
        case (0x2F<<3)|7: assert(false);break;
    /* BMI # */
        case (0x30<<3)|0: _SA(c->PC++);break;
        case (0x30<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x80){_FETCH();}else{_SA(c->PC);};break;
        case (0x30<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0x30<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0x30<<3)|4: assert(false);break;
        case (0x30<<3)|5: assert(false);break;
        case (0x30<<3)|6: assert(false);break;
        case (0x30<<3)|7: assert(false);break;
    /* AND (zp),Y */
        case (0x31<<3)|0: _SA(c->PC++);break;
        case (0x31<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x31<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x31<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x31<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x31<<3)|5: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x31<<3)|6: assert(false);break;
        case (0x31<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x32<<3)|0: _SA(c->PC);break;
        case (0x32<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x32<<3)|2: assert(false);break;
        case (0x32<<3)|3: assert(false);break;
        case (0x32<<3)|4: assert(false);break;
        case (0x32<<3)|5: assert(false);break;
        case (0x32<<3)|6: assert(false);break;
        case (0x32<<3)|7: assert(false);break;
    /* RLA (zp),Y (undoc) */
        case (0x33<<3)|0: _SA(c->PC++);break;
        case (0x33<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x33<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x33<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x33<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x33<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x33<<3)|6: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x33<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0x34<<3)|0: _SA(c->PC++);break;
        case (0x34<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x34<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x34<<3)|3: _FETCH();_ID();break;
        case (0x34<<3)|4: assert(false);break;
        case (0x34<<3)|5: assert(false);break;
        case (0x34<<3)|6: assert(false);break;
        case (0x34<<3)|7: assert(false);break;
    /* AND zp,X */
        case (0x35<<3)|0: _SA(c->PC++);break;
        case (0x35<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x35<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x35<<3)|3: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x35<<3)|4: assert(false);break;
        case (0x35<<3)|5: assert(false);break;
        case (0x35<<3)|6: assert(false);break;
        case (0x35<<3)|7: assert(false);break;
    /* ROL zp,X */
        case (0x36<<3)|0: _SA(c->PC++);break;
        case (0x36<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x36<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x36<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x36<<3)|4: _WR();_SD(_m6502_rol(c,c->AD));_ID();break;
        case (0x36<<3)|5: _FETCH();_ID();break;
        case (0x36<<3)|6: assert(false);break;
        case (0x36<<3)|7: assert(false);break;
    /* RLA zp,X (undoc) */
        case (0x37<<3)|0: _SA(c->PC++);break;
        case (0x37<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x37<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x37<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x37<<3)|4: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x37<<3)|5: _FETCH();_ID();break;
        case (0x37<<3)|6: assert(false);break;
        case (0x37<<3)|7: assert(false);break;
    /* SEC  */
        case (0x38<<3)|0: _SA(c->PC);break;
        case (0x38<<3)|1: _FETCH();c->P|=0x1;_ID();break;
        case (0x38<<3)|2: assert(false);break;
        case (0x38<<3)|3: assert(false);break;
        case (0x38<<3)|4: assert(false);break;
        case (0x38<<3)|5: assert(false);break;
        case (0x38<<3)|6: assert(false);break;
        case (0x38<<3)|7: assert(false);break;
    /* AND abs,Y */
        case (0x39<<3)|0: _SA(c->PC++);break;
        case (0x39<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x39<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x39<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x39<<3)|4: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x39<<3)|5: assert(false);break;
        case (0x39<<3)|6: assert(false);break;
        case (0x39<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0x3A<<3)|0: _SA(c->PC);break;
        case (0x3A<<3)|1: _FETCH();_ID();break;
        case (0x3A<<3)|2: assert(false);break;
        case (0x3A<<3)|3: assert(false);break;
        case (0x3A<<3)|4: assert(false);break;
        case (0x3A<<3)|5: assert(false);break;
        case (0x3A<<3)|6: assert(false);break;
        case (0x3A<<3)|7: assert(false);break;
    /* RLA abs,Y (undoc) */
        case (0x3B<<3)|0: _SA(c->PC++);break;
        case (0x3B<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x3B<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x3B<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x3B<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x3B<<3)|5: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x3B<<3)|6: _FETCH();_ID();break;
        case (0x3B<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0x3C<<3)|0: _SA(c->PC++);break;
        case (0x3C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x3C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x3C<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x3C<<3)|4: _FETCH();_ID();break;
        case (0x3C<<3)|5: assert(false);break;
        case (0x3C<<3)|6: assert(false);break;
        case (0x3C<<3)|7: assert(false);break;
    /* AND abs,X */
        case (0x3D<<3)|0: _SA(c->PC++);break;
        case (0x3D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x3D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x3D<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x3D<<3)|4: _FETCH();c->A&=_GD();_NZ(c->A);break;
        case (0x3D<<3)|5: assert(false);break;
        case (0x3D<<3)|6: assert(false);break;
        case (0x3D<<3)|7: assert(false);break;
    /* ROL abs,X */
        case (0x3E<<3)|0: _SA(c->PC++);break;
        case (0x3E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x3E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x3E<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x3E<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x3E<<3)|5: _WR();_SD(_m6502_rol(c,c->AD));_ID();break;
        case (0x3E<<3)|6: _FETCH();_ID();break;
        case (0x3E<<3)|7: assert(false);break;
    /* RLA abs,X (undoc) */
        case (0x3F<<3)|0: _SA(c->PC++);break;
        case (0x3F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x3F<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x3F<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x3F<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x3F<<3)|5: _WR();c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_ID();break;
        case (0x3F<<3)|6: _FETCH();_ID();break;
        case (0x3F<<3)|7: assert(false);break;
    /* RTI  */
        case (0x40<<3)|0: _SA(c->PC);break;
        case (0x40<<3)|1: _SA(0x0100|c->S++);_ID();break;
        case (0x40<<3)|2: _SA(0x0100|c->S++);_ID();break;
        case (0x40<<3)|3: _SA(0x0100|c->S++);c->P=(_GD()|M6502_BF)&~M6502_XF;break;
        case (0x40<<3)|4: _SA(0x0100|c->S);c->AD=_GD();break;
        case (0x40<<3)|5: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x40<<3)|6: assert(false);break;
        case (0x40<<3)|7: assert(false);break;
    /* EOR (zp,X) */
        case (0x41<<3)|0: _SA(c->PC++);break;
        case (0x41<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x41<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x41<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x41<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x41<<3)|5: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x41<<3)|6: assert(false);break;
        case (0x41<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x42<<3)|0: _SA(c->PC);break;
        case (0x42<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x42<<3)|2: assert(false);break;
        case (0x42<<3)|3: assert(false);break;
        case (0x42<<3)|4: assert(false);break;
        case (0x42<<3)|5: assert(false);break;
        case (0x42<<3)|6: assert(false);break;
        case (0x42<<3)|7: assert(false);break;
    /* SRE (zp,X) (undoc) */
        case (0x43<<3)|0: _SA(c->PC++);break;
        case (0x43<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x43<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x43<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x43<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x43<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x43<<3)|6: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x43<<3)|7: _FETCH();_ID();break;
    /* NOP zp (undoc) */
        case (0x44<<3)|0: _SA(c->PC++);break;
        case (0x44<<3)|1: _SA(_GD());break;
        case (0x44<<3)|2: _FETCH();_ID();break;
        case (0x44<<3)|3: assert(false);break;
        case (0x44<<3)|4: assert(false);break;
        case (0x44<<3)|5: assert(false);break;
        case (0x44<<3)|6: assert(false);break;
        case (0x44<<3)|7: assert(false);break;
    /* EOR zp */
        case (0x45<<3)|0: _SA(c->PC++);break;
        case (0x45<<3)|1: _SA(_GD());break;
        case (0x45<<3)|2: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x45<<3)|3: assert(false);break;
        case (0x45<<3)|4: assert(false);break;
        case (0x45<<3)|5: assert(false);break;
        case (0x45<<3)|6: assert(false);break;
        case (0x45<<3)|7: assert(false);break;
    /* LSR zp */
        case (0x46<<3)|0: _SA(c->PC++);break;
        case (0x46<<3)|1: _SA(_GD());break;
        case (0x46<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x46<<3)|3: _WR();_SD(_m6502_lsr(c,c->AD));_ID();break;
        case (0x46<<3)|4: _FETCH();_ID();break;
        case (0x46<<3)|5: assert(false);break;
        case (0x46<<3)|6: assert(false);break;
        case (0x46<<3)|7: assert(false);break;
    /* SRE zp (undoc) */
        case (0x47<<3)|0: _SA(c->PC++);break;
        case (0x47<<3)|1: _SA(_GD());break;
        case (0x47<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x47<<3)|3: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x47<<3)|4: _FETCH();_ID();break;
        case (0x47<<3)|5: assert(false);break;
        case (0x47<<3)|6: assert(false);break;
        case (0x47<<3)|7: assert(false);break;
    /* PHA  */
        case (0x48<<3)|0: _SA(c->PC);break;
        case (0x48<<3)|1: _WR();_SAD(0x0100|c->S--,c->A);_ID();break;
        case (0x48<<3)|2: _FETCH();_ID();break;
        case (0x48<<3)|3: assert(false);break;
        case (0x48<<3)|4: assert(false);break;
        case (0x48<<3)|5: assert(false);break;
        case (0x48<<3)|6: assert(false);break;
        case (0x48<<3)|7: assert(false);break;
    /* EOR # */
        case (0x49<<3)|0: _SA(c->PC++);break;
        case (0x49<<3)|1: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x49<<3)|2: assert(false);break;
        case (0x49<<3)|3: assert(false);break;
        case (0x49<<3)|4: assert(false);break;
        case (0x49<<3)|5: assert(false);break;
        case (0x49<<3)|6: assert(false);break;
        case (0x49<<3)|7: assert(false);break;
    /* LSRA  */
        case (0x4A<<3)|0: _SA(c->PC);break;
        case (0x4A<<3)|1: _FETCH();c->A=_m6502_lsr(c,c->A);_ID();break;
        case (0x4A<<3)|2: assert(false);break;
        case (0x4A<<3)|3: assert(false);break;
        case (0x4A<<3)|4: assert(false);break;
        case (0x4A<<3)|5: assert(false);break;
        case (0x4A<<3)|6: assert(false);break;
        case (0x4A<<3)|7: assert(false);break;
    /* ASR # (undoc) */
        case (0x4B<<3)|0: _SA(c->PC++);break;
        case (0x4B<<3)|1: _FETCH();c->A&=_GD();c->A=_m6502_lsr(c,c->A);break;
        case (0x4B<<3)|2: assert(false);break;
        case (0x4B<<3)|3: assert(false);break;
        case (0x4B<<3)|4: assert(false);break;
        case (0x4B<<3)|5: assert(false);break;
        case (0x4B<<3)|6: assert(false);break;
        case (0x4B<<3)|7: assert(false);break;
    /* JMP  */
        case (0x4C<<3)|0: _SA(c->PC++);break;
        case (0x4C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x4C<<3)|2: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x4C<<3)|3: assert(false);break;
        case (0x4C<<3)|4: assert(false);break;
        case (0x4C<<3)|5: assert(false);break;
        case (0x4C<<3)|6: assert(false);break;
        case (0x4C<<3)|7: assert(false);break;
    /* EOR abs */
        case (0x4D<<3)|0: _SA(c->PC++);break;
        case (0x4D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x4D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x4D<<3)|3: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x4D<<3)|4: assert(false);break;
        case (0x4D<<3)|5: assert(false);break;
        case (0x4D<<3)|6: assert(false);break;
        case (0x4D<<3)|7: assert(false);break;
    /* LSR abs */
        case (0x4E<<3)|0: _SA(c->PC++);break;
        case (0x4E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x4E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x4E<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x4E<<3)|4: _WR();_SD(_m6502_lsr(c,c->AD));_ID();break;
        case (0x4E<<3)|5: _FETCH();_ID();break;
        case (0x4E<<3)|6: assert(false);break;
        case (0x4E<<3)|7: assert(false);break;
    /* SRE abs (undoc) */
        case (0x4F<<3)|0: _SA(c->PC++);break;
        case (0x4F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x4F<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x4F<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x4F<<3)|4: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x4F<<3)|5: _FETCH();_ID();break;
        case (0x4F<<3)|6: assert(false);break;
        case (0x4F<<3)|7: assert(false);break;
    /* BVC # */
        case (0x50<<3)|0: _SA(c->PC++);break;
        case (0x50<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x0){_FETCH();}else{_SA(c->PC);};break;
        case (0x50<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0x50<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0x50<<3)|4: assert(false);break;
        case (0x50<<3)|5: assert(false);break;
        case (0x50<<3)|6: assert(false);break;
        case (0x50<<3)|7: assert(false);break;
    /* EOR (zp),Y */
        case (0x51<<3)|0: _SA(c->PC++);break;
        case (0x51<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x51<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x51<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x51<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x51<<3)|5: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x51<<3)|6: assert(false);break;
        case (0x51<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x52<<3)|0: _SA(c->PC);break;
        case (0x52<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x52<<3)|2: assert(false);break;
        case (0x52<<3)|3: assert(false);break;
        case (0x52<<3)|4: assert(false);break;
        case (0x52<<3)|5: assert(false);break;
        case (0x52<<3)|6: assert(false);break;
        case (0x52<<3)|7: assert(false);break;
    /* SRE (zp),Y (undoc) */
        case (0x53<<3)|0: _SA(c->PC++);break;
        case (0x53<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x53<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x53<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x53<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x53<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x53<<3)|6: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x53<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0x54<<3)|0: _SA(c->PC++);break;
        case (0x54<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x54<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x54<<3)|3: _FETCH();_ID();break;
        case (0x54<<3)|4: assert(false);break;
        case (0x54<<3)|5: assert(false);break;
        case (0x54<<3)|6: assert(false);break;
        case (0x54<<3)|7: assert(false);break;
    /* EOR zp,X */
        case (0x55<<3)|0: _SA(c->PC++);break;
        case (0x55<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x55<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x55<<3)|3: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x55<<3)|4: assert(false);break;
        case (0x55<<3)|5: assert(false);break;
        case (0x55<<3)|6: assert(false);break;
        case (0x55<<3)|7: assert(false);break;
    /* LSR zp,X */
        case (0x56<<3)|0: _SA(c->PC++);break;
        case (0x56<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x56<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x56<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x56<<3)|4: _WR();_SD(_m6502_lsr(c,c->AD));_ID();break;
        case (0x56<<3)|5: _FETCH();_ID();break;
        case (0x56<<3)|6: assert(false);break;
        case (0x56<<3)|7: assert(false);break;
    /* SRE zp,X (undoc) */
        case (0x57<<3)|0: _SA(c->PC++);break;
        case (0x57<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x57<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x57<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x57<<3)|4: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x57<<3)|5: _FETCH();_ID();break;
        case (0x57<<3)|6: assert(false);break;
        case (0x57<<3)|7: assert(false);break;
    /* CLI  */
        case (0x58<<3)|0: _SA(c->PC);break;
        case (0x58<<3)|1: _FETCH();c->P&=~0x4;_ID();break;
        case (0x58<<3)|2: assert(false);break;
        case (0x58<<3)|3: assert(false);break;
        case (0x58<<3)|4: assert(false);break;
        case (0x58<<3)|5: assert(false);break;
        case (0x58<<3)|6: assert(false);break;
        case (0x58<<3)|7: assert(false);break;
    /* EOR abs,Y */
        case (0x59<<3)|0: _SA(c->PC++);break;
        case (0x59<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x59<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x59<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x59<<3)|4: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x59<<3)|5: assert(false);break;
        case (0x59<<3)|6: assert(false);break;
        case (0x59<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0x5A<<3)|0: _SA(c->PC);break;
        case (0x5A<<3)|1: _FETCH();_ID();break;
        case (0x5A<<3)|2: assert(false);break;
        case (0x5A<<3)|3: assert(false);break;
        case (0x5A<<3)|4: assert(false);break;
        case (0x5A<<3)|5: assert(false);break;
        case (0x5A<<3)|6: assert(false);break;
        case (0x5A<<3)|7: assert(false);break;
    /* SRE abs,Y (undoc) */
        case (0x5B<<3)|0: _SA(c->PC++);break;
        case (0x5B<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x5B<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x5B<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x5B<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x5B<<3)|5: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x5B<<3)|6: _FETCH();_ID();break;
        case (0x5B<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0x5C<<3)|0: _SA(c->PC++);break;
        case (0x5C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x5C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x5C<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x5C<<3)|4: _FETCH();_ID();break;
        case (0x5C<<3)|5: assert(false);break;
        case (0x5C<<3)|6: assert(false);break;
        case (0x5C<<3)|7: assert(false);break;
    /* EOR abs,X */
        case (0x5D<<3)|0: _SA(c->PC++);break;
        case (0x5D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x5D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x5D<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x5D<<3)|4: _FETCH();c->A^=_GD();_NZ(c->A);break;
        case (0x5D<<3)|5: assert(false);break;
        case (0x5D<<3)|6: assert(false);break;
        case (0x5D<<3)|7: assert(false);break;
    /* LSR abs,X */
        case (0x5E<<3)|0: _SA(c->PC++);break;
        case (0x5E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x5E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x5E<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x5E<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x5E<<3)|5: _WR();_SD(_m6502_lsr(c,c->AD));_ID();break;
        case (0x5E<<3)|6: _FETCH();_ID();break;
        case (0x5E<<3)|7: assert(false);break;
    /* SRE abs,X (undoc) */
        case (0x5F<<3)|0: _SA(c->PC++);break;
        case (0x5F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x5F<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x5F<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x5F<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x5F<<3)|5: _WR();c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_ID();break;
        case (0x5F<<3)|6: _FETCH();_ID();break;
        case (0x5F<<3)|7: assert(false);break;
    /* RTS  */
        case (0x60<<3)|0: _SA(c->PC);break;
        case (0x60<<3)|1: _SA(0x0100|c->S++);_ID();break;
        case (0x60<<3)|2: _SA(0x0100|c->S++);_ID();break;
        case (0x60<<3)|3: _SA(0x0100|c->S);c->AD=_GD();break;
        case (0x60<<3)|4: c->PC=(_GD()<<8)|c->AD;_SA(c->PC++);break;
        case (0x60<<3)|5: _FETCH();_ID();break;
        case (0x60<<3)|6: assert(false);break;
        case (0x60<<3)|7: assert(false);break;
    /* ADC (zp,X) */
        case (0x61<<3)|0: _SA(c->PC++);break;
        case (0x61<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x61<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x61<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x61<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x61<<3)|5: _FETCH();_m6502_adc(c,_GD());break;
        case (0x61<<3)|6: assert(false);break;
        case (0x61<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x62<<3)|0: _SA(c->PC);break;
        case (0x62<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x62<<3)|2: assert(false);break;
        case (0x62<<3)|3: assert(false);break;
        case (0x62<<3)|4: assert(false);break;
        case (0x62<<3)|5: assert(false);break;
        case (0x62<<3)|6: assert(false);break;
        case (0x62<<3)|7: assert(false);break;
    /* RRA (zp,X) (undoc) */
        case (0x63<<3)|0: _SA(c->PC++);break;
        case (0x63<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x63<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x63<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x63<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0x63<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x63<<3)|6: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x63<<3)|7: _FETCH();_ID();break;
    /* NOP zp (undoc) */
        case (0x64<<3)|0: _SA(c->PC++);break;
        case (0x64<<3)|1: _SA(_GD());break;
        case (0x64<<3)|2: _FETCH();_ID();break;
        case (0x64<<3)|3: assert(false);break;
        case (0x64<<3)|4: assert(false);break;
        case (0x64<<3)|5: assert(false);break;
        case (0x64<<3)|6: assert(false);break;
        case (0x64<<3)|7: assert(false);break;
    /* ADC zp */
        case (0x65<<3)|0: _SA(c->PC++);break;
        case (0x65<<3)|1: _SA(_GD());break;
        case (0x65<<3)|2: _FETCH();_m6502_adc(c,_GD());break;
        case (0x65<<3)|3: assert(false);break;
        case (0x65<<3)|4: assert(false);break;
        case (0x65<<3)|5: assert(false);break;
        case (0x65<<3)|6: assert(false);break;
        case (0x65<<3)|7: assert(false);break;
    /* ROR zp */
        case (0x66<<3)|0: _SA(c->PC++);break;
        case (0x66<<3)|1: _SA(_GD());break;
        case (0x66<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x66<<3)|3: _WR();_SD(_m6502_ror(c,c->AD));_ID();break;
        case (0x66<<3)|4: _FETCH();_ID();break;
        case (0x66<<3)|5: assert(false);break;
        case (0x66<<3)|6: assert(false);break;
        case (0x66<<3)|7: assert(false);break;
    /* RRA zp (undoc) */
        case (0x67<<3)|0: _SA(c->PC++);break;
        case (0x67<<3)|1: _SA(_GD());break;
        case (0x67<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x67<<3)|3: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x67<<3)|4: _FETCH();_ID();break;
        case (0x67<<3)|5: assert(false);break;
        case (0x67<<3)|6: assert(false);break;
        case (0x67<<3)|7: assert(false);break;
    /* PLA  */
        case (0x68<<3)|0: _SA(c->PC);break;
        case (0x68<<3)|1: _SA(0x0100|c->S++);_ID();break;
        case (0x68<<3)|2: _SA(0x0100|c->S);_ID();break;
        case (0x68<<3)|3: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0x68<<3)|4: assert(false);break;
        case (0x68<<3)|5: assert(false);break;
        case (0x68<<3)|6: assert(false);break;
        case (0x68<<3)|7: assert(false);break;
    /* ADC # */
        case (0x69<<3)|0: _SA(c->PC++);break;
        case (0x69<<3)|1: _FETCH();_m6502_adc(c,_GD());break;
        case (0x69<<3)|2: assert(false);break;
        case (0x69<<3)|3: assert(false);break;
        case (0x69<<3)|4: assert(false);break;
        case (0x69<<3)|5: assert(false);break;
        case (0x69<<3)|6: assert(false);break;
        case (0x69<<3)|7: assert(false);break;
    /* RORA  */
        case (0x6A<<3)|0: _SA(c->PC);break;
        case (0x6A<<3)|1: _FETCH();c->A=_m6502_ror(c,c->A);_ID();break;
        case (0x6A<<3)|2: assert(false);break;
        case (0x6A<<3)|3: assert(false);break;
        case (0x6A<<3)|4: assert(false);break;
        case (0x6A<<3)|5: assert(false);break;
        case (0x6A<<3)|6: assert(false);break;
        case (0x6A<<3)|7: assert(false);break;
    /* ARR # (undoc) */
        case (0x6B<<3)|0: _SA(c->PC++);break;
        case (0x6B<<3)|1: _FETCH();c->A&=_GD();_m6502_arr(c);break;
        case (0x6B<<3)|2: assert(false);break;
        case (0x6B<<3)|3: assert(false);break;
        case (0x6B<<3)|4: assert(false);break;
        case (0x6B<<3)|5: assert(false);break;
        case (0x6B<<3)|6: assert(false);break;
        case (0x6B<<3)|7: assert(false);break;
    /* JMPI  */
        case (0x6C<<3)|0: _SA(c->PC++);break;
        case (0x6C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x6C<<3)|2: c->AD|=_GD()<<8;_SA(c->AD);break;
        case (0x6C<<3)|3: _SA((c->AD&0xFF00)|((c->AD+1)&0x00FF));c->AD=_GD();break;
        case (0x6C<<3)|4: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x6C<<3)|5: assert(false);break;
        case (0x6C<<3)|6: assert(false);break;
        case (0x6C<<3)|7: assert(false);break;
    /* ADC abs */
        case (0x6D<<3)|0: _SA(c->PC++);break;
        case (0x6D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x6D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x6D<<3)|3: _FETCH();_m6502_adc(c,_GD());break;
        case (0x6D<<3)|4: assert(false);break;
        case (0x6D<<3)|5: assert(false);break;
        case (0x6D<<3)|6: assert(false);break;
        case (0x6D<<3)|7: assert(false);break;
    /* ROR abs */
        case (0x6E<<3)|0: _SA(c->PC++);break;
        case (0x6E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x6E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x6E<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x6E<<3)|4: _WR();_SD(_m6502_ror(c,c->AD));_ID();break;
        case (0x6E<<3)|5: _FETCH();_ID();break;
        case (0x6E<<3)|6: assert(false);break;
        case (0x6E<<3)|7: assert(false);break;
    /* RRA abs (undoc) */
        case (0x6F<<3)|0: _SA(c->PC++);break;
        case (0x6F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x6F<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x6F<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x6F<<3)|4: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x6F<<3)|5: _FETCH();_ID();break;
        case (0x6F<<3)|6: assert(false);break;
        case (0x6F<<3)|7: assert(false);break;
    /* BVS # */
        case (0x70<<3)|0: _SA(c->PC++);break;
        case (0x70<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x40){_FETCH();}else{_SA(c->PC);};break;
        case (0x70<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0x70<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0x70<<3)|4: assert(false);break;
        case (0x70<<3)|5: assert(false);break;
        case (0x70<<3)|6: assert(false);break;
        case (0x70<<3)|7: assert(false);break;
    /* ADC (zp),Y */
        case (0x71<<3)|0: _SA(c->PC++);break;
        case (0x71<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x71<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x71<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x71<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x71<<3)|5: _FETCH();_m6502_adc(c,_GD());break;
        case (0x71<<3)|6: assert(false);break;
        case (0x71<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x72<<3)|0: _SA(c->PC);break;
        case (0x72<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x72<<3)|2: assert(false);break;
        case (0x72<<3)|3: assert(false);break;
        case (0x72<<3)|4: assert(false);break;
        case (0x72<<3)|5: assert(false);break;
        case (0x72<<3)|6: assert(false);break;
        case (0x72<<3)|7: assert(false);break;
    /* RRA (zp),Y (undoc) */
        case (0x73<<3)|0: _SA(c->PC++);break;
        case (0x73<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x73<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x73<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x73<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0x73<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x73<<3)|6: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x73<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0x74<<3)|0: _SA(c->PC++);break;
        case (0x74<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x74<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x74<<3)|3: _FETCH();_ID();break;
        case (0x74<<3)|4: assert(false);break;
        case (0x74<<3)|5: assert(false);break;
        case (0x74<<3)|6: assert(false);break;
        case (0x74<<3)|7: assert(false);break;
    /* ADC zp,X */
        case (0x75<<3)|0: _SA(c->PC++);break;
        case (0x75<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x75<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x75<<3)|3: _FETCH();_m6502_adc(c,_GD());break;
        case (0x75<<3)|4: assert(false);break;
        case (0x75<<3)|5: assert(false);break;
        case (0x75<<3)|6: assert(false);break;
        case (0x75<<3)|7: assert(false);break;
    /* ROR zp,X */
        case (0x76<<3)|0: _SA(c->PC++);break;
        case (0x76<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x76<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x76<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x76<<3)|4: _WR();_SD(_m6502_ror(c,c->AD));_ID();break;
        case (0x76<<3)|5: _FETCH();_ID();break;
        case (0x76<<3)|6: assert(false);break;
        case (0x76<<3)|7: assert(false);break;
    /* RRA zp,X (undoc) */
        case (0x77<<3)|0: _SA(c->PC++);break;
        case (0x77<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x77<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0x77<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x77<<3)|4: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x77<<3)|5: _FETCH();_ID();break;
        case (0x77<<3)|6: assert(false);break;
        case (0x77<<3)|7: assert(false);break;
    /* SEI  */
        case (0x78<<3)|0: _SA(c->PC);break;
        case (0x78<<3)|1: _FETCH();c->P|=0x4;_ID();break;
        case (0x78<<3)|2: assert(false);break;
        case (0x78<<3)|3: assert(false);break;
        case (0x78<<3)|4: assert(false);break;
        case (0x78<<3)|5: assert(false);break;
        case (0x78<<3)|6: assert(false);break;
        case (0x78<<3)|7: assert(false);break;
    /* ADC abs,Y */
        case (0x79<<3)|0: _SA(c->PC++);break;
        case (0x79<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x79<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0x79<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x79<<3)|4: _FETCH();_m6502_adc(c,_GD());break;
        case (0x79<<3)|5: assert(false);break;
        case (0x79<<3)|6: assert(false);break;
        case (0x79<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0x7A<<3)|0: _SA(c->PC);break;
        case (0x7A<<3)|1: _FETCH();_ID();break;
        case (0x7A<<3)|2: assert(false);break;
        case (0x7A<<3)|3: assert(false);break;
        case (0x7A<<3)|4: assert(false);break;
        case (0x7A<<3)|5: assert(false);break;
        case (0x7A<<3)|6: assert(false);break;
        case (0x7A<<3)|7: assert(false);break;
    /* RRA abs,Y (undoc) */
        case (0x7B<<3)|0: _SA(c->PC++);break;
        case (0x7B<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x7B<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x7B<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0x7B<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x7B<<3)|5: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x7B<<3)|6: _FETCH();_ID();break;
        case (0x7B<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0x7C<<3)|0: _SA(c->PC++);break;
        case (0x7C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x7C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x7C<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x7C<<3)|4: _FETCH();_ID();break;
        case (0x7C<<3)|5: assert(false);break;
        case (0x7C<<3)|6: assert(false);break;
        case (0x7C<<3)|7: assert(false);break;
    /* ADC abs,X */
        case (0x7D<<3)|0: _SA(c->PC++);break;
        case (0x7D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x7D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0x7D<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x7D<<3)|4: _FETCH();_m6502_adc(c,_GD());break;
        case (0x7D<<3)|5: assert(false);break;
        case (0x7D<<3)|6: assert(false);break;
        case (0x7D<<3)|7: assert(false);break;
    /* ROR abs,X */
        case (0x7E<<3)|0: _SA(c->PC++);break;
        case (0x7E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x7E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x7E<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x7E<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x7E<<3)|5: _WR();_SD(_m6502_ror(c,c->AD));_ID();break;
        case (0x7E<<3)|6: _FETCH();_ID();break;
        case (0x7E<<3)|7: assert(false);break;
    /* RRA abs,X (undoc) */
        case (0x7F<<3)|0: _SA(c->PC++);break;
        case (0x7F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x7F<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x7F<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0x7F<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0x7F<<3)|5: _WR();c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_ID();break;
        case (0x7F<<3)|6: _FETCH();_ID();break;
        case (0x7F<<3)|7: assert(false);break;
    /* NOP # (undoc) */
        case (0x80<<3)|0: _SA(c->PC++);break;
        case (0x80<<3)|1: _FETCH();_ID();break;
        case (0x80<<3)|2: assert(false);break;
        case (0x80<<3)|3: assert(false);break;
        case (0x80<<3)|4: assert(false);break;
        case (0x80<<3)|5: assert(false);break;
        case (0x80<<3)|6: assert(false);break;
        case (0x80<<3)|7: assert(false);break;
    /* STA (zp,X) */
        case (0x81<<3)|0: _SA(c->PC++);break;
        case (0x81<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x81<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x81<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x81<<3)|4: _WR();_SA((_GD()<<8)|c->AD);_SD(c->A);break;
        case (0x81<<3)|5: _FETCH();_ID();break;
        case (0x81<<3)|6: assert(false);break;
        case (0x81<<3)|7: assert(false);break;
    /* NOP # (undoc) */
        case (0x82<<3)|0: _SA(c->PC++);break;
        case (0x82<<3)|1: _FETCH();_ID();break;
        case (0x82<<3)|2: assert(false);break;
        case (0x82<<3)|3: assert(false);break;
        case (0x82<<3)|4: assert(false);break;
        case (0x82<<3)|5: assert(false);break;
        case (0x82<<3)|6: assert(false);break;
        case (0x82<<3)|7: assert(false);break;
    /* SAX (zp,X) (undoc) */
        case (0x83<<3)|0: _SA(c->PC++);break;
        case (0x83<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x83<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0x83<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x83<<3)|4: _WR();_SA((_GD()<<8)|c->AD);_SD(c->A&c->X);break;
        case (0x83<<3)|5: _FETCH();_ID();break;
        case (0x83<<3)|6: assert(false);break;
        case (0x83<<3)|7: assert(false);break;
    /* STY zp */
        case (0x84<<3)|0: _SA(c->PC++);break;
        case (0x84<<3)|1: _WR();_SA(_GD());_SD(c->Y);break;
        case (0x84<<3)|2: _FETCH();_ID();break;
        case (0x84<<3)|3: assert(false);break;
        case (0x84<<3)|4: assert(false);break;
        case (0x84<<3)|5: assert(false);break;
        case (0x84<<3)|6: assert(false);break;
        case (0x84<<3)|7: assert(false);break;
    /* STA zp */
        case (0x85<<3)|0: _SA(c->PC++);break;
        case (0x85<<3)|1: _WR();_SA(_GD());_SD(c->A);break;
        case (0x85<<3)|2: _FETCH();_ID();break;
        case (0x85<<3)|3: assert(false);break;
        case (0x85<<3)|4: assert(false);break;
        case (0x85<<3)|5: assert(false);break;
        case (0x85<<3)|6: assert(false);break;
        case (0x85<<3)|7: assert(false);break;
    /* STX zp */
        case (0x86<<3)|0: _SA(c->PC++);break;
        case (0x86<<3)|1: _WR();_SA(_GD());_SD(c->X);break;
        case (0x86<<3)|2: _FETCH();_ID();break;
        case (0x86<<3)|3: assert(false);break;
        case (0x86<<3)|4: assert(false);break;
        case (0x86<<3)|5: assert(false);break;
        case (0x86<<3)|6: assert(false);break;
        case (0x86<<3)|7: assert(false);break;
    /* SAX zp (undoc) */
        case (0x87<<3)|0: _SA(c->PC++);break;
        case (0x87<<3)|1: _WR();_SA(_GD());_SD(c->A&c->X);break;
        case (0x87<<3)|2: _FETCH();_ID();break;
        case (0x87<<3)|3: assert(false);break;
        case (0x87<<3)|4: assert(false);break;
        case (0x87<<3)|5: assert(false);break;
        case (0x87<<3)|6: assert(false);break;
        case (0x87<<3)|7: assert(false);break;
    /* DEY  */
        case (0x88<<3)|0: _SA(c->PC);break;
        case (0x88<<3)|1: _FETCH();c->Y--;_NZ(c->Y);_ID();break;
        case (0x88<<3)|2: assert(false);break;
        case (0x88<<3)|3: assert(false);break;
        case (0x88<<3)|4: assert(false);break;
        case (0x88<<3)|5: assert(false);break;
        case (0x88<<3)|6: assert(false);break;
        case (0x88<<3)|7: assert(false);break;
    /* NOP # (undoc) */
        case (0x89<<3)|0: _SA(c->PC++);break;
        case (0x89<<3)|1: _FETCH();_ID();break;
        case (0x89<<3)|2: assert(false);break;
        case (0x89<<3)|3: assert(false);break;
        case (0x89<<3)|4: assert(false);break;
        case (0x89<<3)|5: assert(false);break;
        case (0x89<<3)|6: assert(false);break;
        case (0x89<<3)|7: assert(false);break;
    /* TXA  */
        case (0x8A<<3)|0: _SA(c->PC);break;
        case (0x8A<<3)|1: _FETCH();c->A=c->X;_NZ(c->A);_ID();break;
        case (0x8A<<3)|2: assert(false);break;
        case (0x8A<<3)|3: assert(false);break;
        case (0x8A<<3)|4: assert(false);break;
        case (0x8A<<3)|5: assert(false);break;
        case (0x8A<<3)|6: assert(false);break;
        case (0x8A<<3)|7: assert(false);break;
    /* ANE # (undoc) */
        case (0x8B<<3)|0: _SA(c->PC++);break;
        case (0x8B<<3)|1: _FETCH();c->A=(c->A|0xEE)&c->X&_GD();_NZ(c->A);break;
        case (0x8B<<3)|2: assert(false);break;
        case (0x8B<<3)|3: assert(false);break;
        case (0x8B<<3)|4: assert(false);break;
        case (0x8B<<3)|5: assert(false);break;
        case (0x8B<<3)|6: assert(false);break;
        case (0x8B<<3)|7: assert(false);break;
    /* STY abs */
        case (0x8C<<3)|0: _SA(c->PC++);break;
        case (0x8C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x8C<<3)|2: _WR();_SA((_GD()<<8)|c->AD);_SD(c->Y);break;
        case (0x8C<<3)|3: _FETCH();_ID();break;
        case (0x8C<<3)|4: assert(false);break;
        case (0x8C<<3)|5: assert(false);break;
        case (0x8C<<3)|6: assert(false);break;
        case (0x8C<<3)|7: assert(false);break;
    /* STA abs */
        case (0x8D<<3)|0: _SA(c->PC++);break;
        case (0x8D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x8D<<3)|2: _WR();_SA((_GD()<<8)|c->AD);_SD(c->A);break;
        case (0x8D<<3)|3: _FETCH();_ID();break;
        case (0x8D<<3)|4: assert(false);break;
        case (0x8D<<3)|5: assert(false);break;
        case (0x8D<<3)|6: assert(false);break;
        case (0x8D<<3)|7: assert(false);break;
    /* STX abs */
        case (0x8E<<3)|0: _SA(c->PC++);break;
        case (0x8E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x8E<<3)|2: _WR();_SA((_GD()<<8)|c->AD);_SD(c->X);break;
        case (0x8E<<3)|3: _FETCH();_ID();break;
        case (0x8E<<3)|4: assert(false);break;
        case (0x8E<<3)|5: assert(false);break;
        case (0x8E<<3)|6: assert(false);break;
        case (0x8E<<3)|7: assert(false);break;
    /* SAX abs (undoc) */
        case (0x8F<<3)|0: _SA(c->PC++);break;
        case (0x8F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x8F<<3)|2: _WR();_SA((_GD()<<8)|c->AD);_SD(c->A&c->X);break;
        case (0x8F<<3)|3: _FETCH();_ID();break;
        case (0x8F<<3)|4: assert(false);break;
        case (0x8F<<3)|5: assert(false);break;
        case (0x8F<<3)|6: assert(false);break;
        case (0x8F<<3)|7: assert(false);break;
    /* BCC # */
        case (0x90<<3)|0: _SA(c->PC++);break;
        case (0x90<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x0){_FETCH();}else{_SA(c->PC);};break;
        case (0x90<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0x90<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0x90<<3)|4: assert(false);break;
        case (0x90<<3)|5: assert(false);break;
        case (0x90<<3)|6: assert(false);break;
        case (0x90<<3)|7: assert(false);break;
    /* STA (zp),Y */
        case (0x91<<3)|0: _SA(c->PC++);break;
        case (0x91<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x91<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x91<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x91<<3)|4: _WR();_SA(c->AD+c->Y);_SD(c->A);_ID();break;
        case (0x91<<3)|5: _FETCH();_ID();break;
        case (0x91<<3)|6: assert(false);break;
        case (0x91<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0x92<<3)|0: _SA(c->PC);break;
        case (0x92<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0x92<<3)|2: assert(false);break;
        case (0x92<<3)|3: assert(false);break;
        case (0x92<<3)|4: assert(false);break;
        case (0x92<<3)|5: assert(false);break;
        case (0x92<<3)|6: assert(false);break;
        case (0x92<<3)|7: assert(false);break;
    /* SHA (zp),Y (undoc) */
        case (0x93<<3)|0: _SA(c->PC++);break;
        case (0x93<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x93<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x93<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x93<<3)|4: _WR();_SA(c->AD+c->Y);_SD(c->A&c->X&(uint8_t)((_GA()>>8)+1));_ID();break;
        case (0x93<<3)|5: _FETCH();_ID();break;
        case (0x93<<3)|6: assert(false);break;
        case (0x93<<3)|7: assert(false);break;
    /* STY zp,X */
        case (0x94<<3)|0: _SA(c->PC++);break;
        case (0x94<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x94<<3)|2: _WR();_SA((c->AD+c->X)&0x00FF);_SD(c->Y);_ID();break;
        case (0x94<<3)|3: _FETCH();_ID();break;
        case (0x94<<3)|4: assert(false);break;
        case (0x94<<3)|5: assert(false);break;
        case (0x94<<3)|6: assert(false);break;
        case (0x94<<3)|7: assert(false);break;
    /* STA zp,X */
        case (0x95<<3)|0: _SA(c->PC++);break;
        case (0x95<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x95<<3)|2: _WR();_SA((c->AD+c->X)&0x00FF);_SD(c->A);_ID();break;
        case (0x95<<3)|3: _FETCH();_ID();break;
        case (0x95<<3)|4: assert(false);break;
        case (0x95<<3)|5: assert(false);break;
        case (0x95<<3)|6: assert(false);break;
        case (0x95<<3)|7: assert(false);break;
    /* STX zp,Y */
        case (0x96<<3)|0: _SA(c->PC++);break;
        case (0x96<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x96<<3)|2: _WR();_SA((c->AD+c->Y)&0x00FF);_SD(c->X);_ID();break;
        case (0x96<<3)|3: _FETCH();_ID();break;
        case (0x96<<3)|4: assert(false);break;
        case (0x96<<3)|5: assert(false);break;
        case (0x96<<3)|6: assert(false);break;
        case (0x96<<3)|7: assert(false);break;
    /* SAX zp,Y (undoc) */
        case (0x97<<3)|0: _SA(c->PC++);break;
        case (0x97<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x97<<3)|2: _WR();_SA((c->AD+c->Y)&0x00FF);_SD(c->A&c->X);_ID();break;
        case (0x97<<3)|3: _FETCH();_ID();break;
        case (0x97<<3)|4: assert(false);break;
        case (0x97<<3)|5: assert(false);break;
        case (0x97<<3)|6: assert(false);break;
        case (0x97<<3)|7: assert(false);break;
    /* TYA  */
        case (0x98<<3)|0: _SA(c->PC);break;
        case (0x98<<3)|1: _FETCH();c->A=c->Y;_NZ(c->A);_ID();break;
        case (0x98<<3)|2: assert(false);break;
        case (0x98<<3)|3: assert(false);break;
        case (0x98<<3)|4: assert(false);break;
        case (0x98<<3)|5: assert(false);break;
        case (0x98<<3)|6: assert(false);break;
        case (0x98<<3)|7: assert(false);break;
    /* STA abs,Y */
        case (0x99<<3)|0: _SA(c->PC++);break;
        case (0x99<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x99<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x99<<3)|3: _WR();_SA(c->AD+c->Y);_SD(c->A);_ID();break;
        case (0x99<<3)|4: _FETCH();_ID();break;
        case (0x99<<3)|5: assert(false);break;
        case (0x99<<3)|6: assert(false);break;
        case (0x99<<3)|7: assert(false);break;
    /* TXS  */
        case (0x9A<<3)|0: _SA(c->PC);break;
        case (0x9A<<3)|1: _FETCH();c->S=c->X;_ID();break;
        case (0x9A<<3)|2: assert(false);break;
        case (0x9A<<3)|3: assert(false);break;
        case (0x9A<<3)|4: assert(false);break;
        case (0x9A<<3)|5: assert(false);break;
        case (0x9A<<3)|6: assert(false);break;
        case (0x9A<<3)|7: assert(false);break;
    /* SHS abs,Y (undoc) */
        case (0x9B<<3)|0: _SA(c->PC++);break;
        case (0x9B<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x9B<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x9B<<3)|3: _WR();_SA(c->AD+c->Y);c->S=c->A&c->X;_SD(c->S&(uint8_t)((_GA()>>8)+1));_ID();break;
        case (0x9B<<3)|4: _FETCH();_ID();break;
        case (0x9B<<3)|5: assert(false);break;
        case (0x9B<<3)|6: assert(false);break;
        case (0x9B<<3)|7: assert(false);break;
    /* SHY abs,X (undoc) */
        case (0x9C<<3)|0: _SA(c->PC++);break;
        case (0x9C<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x9C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x9C<<3)|3: _WR();_SA(c->AD+c->X);_SD(c->Y&(uint8_t)((_GA()>>8)+1));_ID();break;
        case (0x9C<<3)|4: _FETCH();_ID();break;
        case (0x9C<<3)|5: assert(false);break;
        case (0x9C<<3)|6: assert(false);break;
        case (0x9C<<3)|7: assert(false);break;
    /* STA abs,X */
        case (0x9D<<3)|0: _SA(c->PC++);break;
        case (0x9D<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x9D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0x9D<<3)|3: _WR();_SA(c->AD+c->X);_SD(c->A);_ID();break;
        case (0x9D<<3)|4: _FETCH();_ID();break;
        case (0x9D<<3)|5: assert(false);break;
        case (0x9D<<3)|6: assert(false);break;
        case (0x9D<<3)|7: assert(false);break;
    /* SHX abs,Y (undoc) */
        case (0x9E<<3)|0: _SA(c->PC++);break;
        case (0x9E<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x9E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x9E<<3)|3: _WR();_SA(c->AD+c->Y);_SD(c->X&(uint8_t)((_GA()>>8)+1));_ID();break;
        case (0x9E<<3)|4: _FETCH();_ID();break;
        case (0x9E<<3)|5: assert(false);break;
        case (0x9E<<3)|6: assert(false);break;
        case (0x9E<<3)|7: assert(false);break;
    /* SHA abs,Y (undoc) */
        case (0x9F<<3)|0: _SA(c->PC++);break;
        case (0x9F<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0x9F<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0x9F<<3)|3: _WR();_SA(c->AD+c->Y);_SD(c->A&c->X&(uint8_t)((_GA()>>8)+1));_ID();break;
        case (0x9F<<3)|4: _FETCH();_ID();break;
        case (0x9F<<3)|5: assert(false);break;
        case (0x9F<<3)|6: assert(false);break;
        case (0x9F<<3)|7: assert(false);break;
    /* LDY # */
        case (0xA0<<3)|0: _SA(c->PC++);break;
        case (0xA0<<3)|1: _FETCH();c->Y=_GD();_NZ(c->Y);break;
        case (0xA0<<3)|2: assert(false);break;
        case (0xA0<<3)|3: assert(false);break;
        case (0xA0<<3)|4: assert(false);break;
        case (0xA0<<3)|5: assert(false);break;
        case (0xA0<<3)|6: assert(false);break;
        case (0xA0<<3)|7: assert(false);break;
    /* LDA (zp,X) */
        case (0xA1<<3)|0: _SA(c->PC++);break;
        case (0xA1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xA1<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xA1<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xA1<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xA1<<3)|5: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xA1<<3)|6: assert(false);break;
        case (0xA1<<3)|7: assert(false);break;
    /* LDX # */
        case (0xA2<<3)|0: _SA(c->PC++);break;
        case (0xA2<<3)|1: _FETCH();c->X=_GD();_NZ(c->X);break;
        case (0xA2<<3)|2: assert(false);break;
        case (0xA2<<3)|3: assert(false);break;
        case (0xA2<<3)|4: assert(false);break;
        case (0xA2<<3)|5: assert(false);break;
        case (0xA2<<3)|6: assert(false);break;
        case (0xA2<<3)|7: assert(false);break;
    /* LAX (zp,X) (undoc) */
        case (0xA3<<3)|0: _SA(c->PC++);break;
        case (0xA3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xA3<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xA3<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xA3<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xA3<<3)|5: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xA3<<3)|6: assert(false);break;
        case (0xA3<<3)|7: assert(false);break;
    /* LDY zp */
        case (0xA4<<3)|0: _SA(c->PC++);break;
        case (0xA4<<3)|1: _SA(_GD());break;
        case (0xA4<<3)|2: _FETCH();c->Y=_GD();_NZ(c->Y);break;
        case (0xA4<<3)|3: assert(false);break;
        case (0xA4<<3)|4: assert(false);break;
        case (0xA4<<3)|5: assert(false);break;
        case (0xA4<<3)|6: assert(false);break;
        case (0xA4<<3)|7: assert(false);break;
    /* LDA zp */
        case (0xA5<<3)|0: _SA(c->PC++);break;
        case (0xA5<<3)|1: _SA(_GD());break;
        case (0xA5<<3)|2: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xA5<<3)|3: assert(false);break;
        case (0xA5<<3)|4: assert(false);break;
        case (0xA5<<3)|5: assert(false);break;
        case (0xA5<<3)|6: assert(false);break;
        case (0xA5<<3)|7: assert(false);break;
    /* LDX zp */
        case (0xA6<<3)|0: _SA(c->PC++);break;
        case (0xA6<<3)|1: _SA(_GD());break;
        case (0xA6<<3)|2: _FETCH();c->X=_GD();_NZ(c->X);break;
        case (0xA6<<3)|3: assert(false);break;
        case (0xA6<<3)|4: assert(false);break;
        case (0xA6<<3)|5: assert(false);break;
        case (0xA6<<3)|6: assert(false);break;
        case (0xA6<<3)|7: assert(false);break;
    /* LAX zp (undoc) */
        case (0xA7<<3)|0: _SA(c->PC++);break;
        case (0xA7<<3)|1: _SA(_GD());break;
        case (0xA7<<3)|2: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xA7<<3)|3: assert(false);break;
        case (0xA7<<3)|4: assert(false);break;
        case (0xA7<<3)|5: assert(false);break;
        case (0xA7<<3)|6: assert(false);break;
        case (0xA7<<3)|7: assert(false);break;
    /* TAY  */
        case (0xA8<<3)|0: _SA(c->PC);break;
        case (0xA8<<3)|1: _FETCH();c->Y=c->A;_NZ(c->Y);_ID();break;
        case (0xA8<<3)|2: assert(false);break;
        case (0xA8<<3)|3: assert(false);break;
        case (0xA8<<3)|4: assert(false);break;
        case (0xA8<<3)|5: assert(false);break;
        case (0xA8<<3)|6: assert(false);break;
        case (0xA8<<3)|7: assert(false);break;
    /* LDA # */
        case (0xA9<<3)|0: _SA(c->PC++);break;
        case (0xA9<<3)|1: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xA9<<3)|2: assert(false);break;
        case (0xA9<<3)|3: assert(false);break;
        case (0xA9<<3)|4: assert(false);break;
        case (0xA9<<3)|5: assert(false);break;
        case (0xA9<<3)|6: assert(false);break;
        case (0xA9<<3)|7: assert(false);break;
    /* TAX  */
        case (0xAA<<3)|0: _SA(c->PC);break;
        case (0xAA<<3)|1: _FETCH();c->X=c->A;_NZ(c->X);_ID();break;
        case (0xAA<<3)|2: assert(false);break;
        case (0xAA<<3)|3: assert(false);break;
        case (0xAA<<3)|4: assert(false);break;
        case (0xAA<<3)|5: assert(false);break;
        case (0xAA<<3)|6: assert(false);break;
        case (0xAA<<3)|7: assert(false);break;
    /* LXA # (undoc) */
        case (0xAB<<3)|0: _SA(c->PC++);break;
        case (0xAB<<3)|1: _FETCH();c->A=c->X=(c->A|0xEE)&_GD();_NZ(c->A);break;
        case (0xAB<<3)|2: assert(false);break;
        case (0xAB<<3)|3: assert(false);break;
        case (0xAB<<3)|4: assert(false);break;
        case (0xAB<<3)|5: assert(false);break;
        case (0xAB<<3)|6: assert(false);break;
        case (0xAB<<3)|7: assert(false);break;
    /* LDY abs */
        case (0xAC<<3)|0: _SA(c->PC++);break;
        case (0xAC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xAC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAC<<3)|3: _FETCH();c->Y=_GD();_NZ(c->Y);break;
        case (0xAC<<3)|4: assert(false);break;
        case (0xAC<<3)|5: assert(false);break;
        case (0xAC<<3)|6: assert(false);break;
        case (0xAC<<3)|7: assert(false);break;
    /* LDA abs */
        case (0xAD<<3)|0: _SA(c->PC++);break;
        case (0xAD<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xAD<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAD<<3)|3: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xAD<<3)|4: assert(false);break;
        case (0xAD<<3)|5: assert(false);break;
        case (0xAD<<3)|6: assert(false);break;
        case (0xAD<<3)|7: assert(false);break;
    /* LDX abs */
        case (0xAE<<3)|0: _SA(c->PC++);break;
        case (0xAE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xAE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAE<<3)|3: _FETCH();c->X=_GD();_NZ(c->X);break;
        case (0xAE<<3)|4: assert(false);break;
        case (0xAE<<3)|5: assert(false);break;
        case (0xAE<<3)|6: assert(false);break;
        case (0xAE<<3)|7: assert(false);break;
    /* LAX abs (undoc) */
        case (0xAF<<3)|0: _SA(c->PC++);break;
        case (0xAF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xAF<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAF<<3)|3: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xAF<<3)|4: assert(false);break;
        case (0xAF<<3)|5: assert(false);break;
        case (0xAF<<3)|6: assert(false);break;
        case (0xAF<<3)|7: assert(false);break;
    /* BCS # */
        case (0xB0<<3)|0: _SA(c->PC++);break;
        case (0xB0<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x1){_FETCH();}else{_SA(c->PC);};break;
        case (0xB0<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0xB0<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0xB0<<3)|4: assert(false);break;
        case (0xB0<<3)|5: assert(false);break;
        case (0xB0<<3)|6: assert(false);break;
        case (0xB0<<3)|7: assert(false);break;
    /* LDA (zp),Y */
        case (0xB1<<3)|0: _SA(c->PC++);break;
        case (0xB1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB1<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xB1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xB1<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xB1<<3)|5: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xB1<<3)|6: assert(false);break;
        case (0xB1<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0xB2<<3)|0: _SA(c->PC);break;
        case (0xB2<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0xB2<<3)|2: assert(false);break;
        case (0xB2<<3)|3: assert(false);break;
        case (0xB2<<3)|4: assert(false);break;
        case (0xB2<<3)|5: assert(false);break;
        case (0xB2<<3)|6: assert(false);break;
        case (0xB2<<3)|7: assert(false);break;
    /* LAX (zp),Y (undoc) */
        case (0xB3<<3)|0: _SA(c->PC++);break;
        case (0xB3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB3<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xB3<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xB3<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xB3<<3)|5: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xB3<<3)|6: assert(false);break;
        case (0xB3<<3)|7: assert(false);break;
    /* LDY zp,X */
        case (0xB4<<3)|0: _SA(c->PC++);break;
        case (0xB4<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB4<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xB4<<3)|3: _FETCH();c->Y=_GD();_NZ(c->Y);break;
        case (0xB4<<3)|4: assert(false);break;
        case (0xB4<<3)|5: assert(false);break;
        case (0xB4<<3)|6: assert(false);break;
        case (0xB4<<3)|7: assert(false);break;
    /* LDA zp,X */
        case (0xB5<<3)|0: _SA(c->PC++);break;
        case (0xB5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB5<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xB5<<3)|3: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xB5<<3)|4: assert(false);break;
        case (0xB5<<3)|5: assert(false);break;
        case (0xB5<<3)|6: assert(false);break;
        case (0xB5<<3)|7: assert(false);break;
    /* LDX zp,Y */
        case (0xB6<<3)|0: _SA(c->PC++);break;
        case (0xB6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB6<<3)|2: _SA((c->AD+c->Y)&0x00FF);_ID();break;
        case (0xB6<<3)|3: _FETCH();c->X=_GD();_NZ(c->X);break;
        case (0xB6<<3)|4: assert(false);break;
        case (0xB6<<3)|5: assert(false);break;
        case (0xB6<<3)|6: assert(false);break;
        case (0xB6<<3)|7: assert(false);break;
    /* LAX zp,Y (undoc) */
        case (0xB7<<3)|0: _SA(c->PC++);break;
        case (0xB7<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB7<<3)|2: _SA((c->AD+c->Y)&0x00FF);_ID();break;
        case (0xB7<<3)|3: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xB7<<3)|4: assert(false);break;
        case (0xB7<<3)|5: assert(false);break;
        case (0xB7<<3)|6: assert(false);break;
        case (0xB7<<3)|7: assert(false);break;
    /* CLV  */
        case (0xB8<<3)|0: _SA(c->PC);break;
        case (0xB8<<3)|1: _FETCH();c->P&=~0x40;_ID();break;
        case (0xB8<<3)|2: assert(false);break;
        case (0xB8<<3)|3: assert(false);break;
        case (0xB8<<3)|4: assert(false);break;
        case (0xB8<<3)|5: assert(false);break;
        case (0xB8<<3)|6: assert(false);break;
        case (0xB8<<3)|7: assert(false);break;
    /* LDA abs,Y */
        case (0xB9<<3)|0: _SA(c->PC++);break;
        case (0xB9<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xB9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xB9<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xB9<<3)|4: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xB9<<3)|5: assert(false);break;
        case (0xB9<<3)|6: assert(false);break;
        case (0xB9<<3)|7: assert(false);break;
    /* TSX  */
        case (0xBA<<3)|0: _SA(c->PC);break;
        case (0xBA<<3)|1: _FETCH();c->X=c->S;_NZ(c->X);_ID();break;
        case (0xBA<<3)|2: assert(false);break;
        case (0xBA<<3)|3: assert(false);break;
        case (0xBA<<3)|4: assert(false);break;
        case (0xBA<<3)|5: assert(false);break;
        case (0xBA<<3)|6: assert(false);break;
        case (0xBA<<3)|7: assert(false);break;
    /* LAS abs,Y (undoc) */
        case (0xBB<<3)|0: _SA(c->PC++);break;
        case (0xBB<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xBB<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xBB<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xBB<<3)|4: _FETCH();c->A=c->X=c->S=_GD()&c->S;_NZ(c->A);break;
        case (0xBB<<3)|5: assert(false);break;
        case (0xBB<<3)|6: assert(false);break;
        case (0xBB<<3)|7: assert(false);break;
    /* LDY abs,X */
        case (0xBC<<3)|0: _SA(c->PC++);break;
        case (0xBC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xBC<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xBC<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xBC<<3)|4: _FETCH();c->Y=_GD();_NZ(c->Y);break;
        case (0xBC<<3)|5: assert(false);break;
        case (0xBC<<3)|6: assert(false);break;
        case (0xBC<<3)|7: assert(false);break;
    /* LDA abs,X */
        case (0xBD<<3)|0: _SA(c->PC++);break;
        case (0xBD<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xBD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xBD<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xBD<<3)|4: _FETCH();c->A=_GD();_NZ(c->A);break;
        case (0xBD<<3)|5: assert(false);break;
        case (0xBD<<3)|6: assert(false);break;
        case (0xBD<<3)|7: assert(false);break;
    /* LDX abs,Y */
        case (0xBE<<3)|0: _SA(c->PC++);break;
        case (0xBE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xBE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xBE<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xBE<<3)|4: _FETCH();c->X=_GD();_NZ(c->X);break;
        case (0xBE<<3)|5: assert(false);break;
        case (0xBE<<3)|6: assert(false);break;
        case (0xBE<<3)|7: assert(false);break;
    /* LAX abs,Y (undoc) */
        case (0xBF<<3)|0: _SA(c->PC++);break;
        case (0xBF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xBF<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xBF<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xBF<<3)|4: _FETCH();c->A=c->X=_GD();_NZ(c->A);break;
        case (0xBF<<3)|5: assert(false);break;
        case (0xBF<<3)|6: assert(false);break;
        case (0xBF<<3)|7: assert(false);break;
    /* CPY # */
        case (0xC0<<3)|0: _SA(c->PC++);break;
        case (0xC0<<3)|1: _FETCH();_m6502_cmp(c, c->Y, _GD());break;
        case (0xC0<<3)|2: assert(false);break;
        case (0xC0<<3)|3: assert(false);break;
        case (0xC0<<3)|4: assert(false);break;
        case (0xC0<<3)|5: assert(false);break;
        case (0xC0<<3)|6: assert(false);break;
        case (0xC0<<3)|7: assert(false);break;
    /* CMP (zp,X) */
        case (0xC1<<3)|0: _SA(c->PC++);break;
        case (0xC1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xC1<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xC1<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xC1<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xC1<<3)|5: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xC1<<3)|6: assert(false);break;
        case (0xC1<<3)|7: assert(false);break;
    /* NOP # (undoc) */
        case (0xC2<<3)|0: _SA(c->PC++);break;
        case (0xC2<<3)|1: _FETCH();_ID();break;
        case (0xC2<<3)|2: assert(false);break;
        case (0xC2<<3)|3: assert(false);break;
        case (0xC2<<3)|4: assert(false);break;
        case (0xC2<<3)|5: assert(false);break;
        case (0xC2<<3)|6: assert(false);break;
        case (0xC2<<3)|7: assert(false);break;
    /* DCP (zp,X) (undoc) */
        case (0xC3<<3)|0: _SA(c->PC++);break;
        case (0xC3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xC3<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xC3<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xC3<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xC3<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xC3<<3)|6: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xC3<<3)|7: _FETCH();_ID();break;
    /* CPY zp */
        case (0xC4<<3)|0: _SA(c->PC++);break;
        case (0xC4<<3)|1: _SA(_GD());break;
        case (0xC4<<3)|2: _FETCH();_m6502_cmp(c, c->Y, _GD());break;
        case (0xC4<<3)|3: assert(false);break;
        case (0xC4<<3)|4: assert(false);break;
        case (0xC4<<3)|5: assert(false);break;
        case (0xC4<<3)|6: assert(false);break;
        case (0xC4<<3)|7: assert(false);break;
    /* CMP zp */
        case (0xC5<<3)|0: _SA(c->PC++);break;
        case (0xC5<<3)|1: _SA(_GD());break;
        case (0xC5<<3)|2: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xC5<<3)|3: assert(false);break;
        case (0xC5<<3)|4: assert(false);break;
        case (0xC5<<3)|5: assert(false);break;
        case (0xC5<<3)|6: assert(false);break;
        case (0xC5<<3)|7: assert(false);break;
    /* DEC zp */
        case (0xC6<<3)|0: _SA(c->PC++);break;
        case (0xC6<<3)|1: _SA(_GD());break;
        case (0xC6<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xC6<<3)|3: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xC6<<3)|4: _FETCH();_ID();break;
        case (0xC6<<3)|5: assert(false);break;
        case (0xC6<<3)|6: assert(false);break;
        case (0xC6<<3)|7: assert(false);break;
    /* DCP zp (undoc) */
        case (0xC7<<3)|0: _SA(c->PC++);break;
        case (0xC7<<3)|1: _SA(_GD());break;
        case (0xC7<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xC7<<3)|3: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xC7<<3)|4: _FETCH();_ID();break;
        case (0xC7<<3)|5: assert(false);break;
        case (0xC7<<3)|6: assert(false);break;
        case (0xC7<<3)|7: assert(false);break;
    /* INY  */
        case (0xC8<<3)|0: _SA(c->PC);break;
        case (0xC8<<3)|1: _FETCH();c->Y++;_NZ(c->Y);_ID();break;
        case (0xC8<<3)|2: assert(false);break;
        case (0xC8<<3)|3: assert(false);break;
        case (0xC8<<3)|4: assert(false);break;
        case (0xC8<<3)|5: assert(false);break;
        case (0xC8<<3)|6: assert(false);break;
        case (0xC8<<3)|7: assert(false);break;
    /* CMP # */
        case (0xC9<<3)|0: _SA(c->PC++);break;
        case (0xC9<<3)|1: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xC9<<3)|2: assert(false);break;
        case (0xC9<<3)|3: assert(false);break;
        case (0xC9<<3)|4: assert(false);break;
        case (0xC9<<3)|5: assert(false);break;
        case (0xC9<<3)|6: assert(false);break;
        case (0xC9<<3)|7: assert(false);break;
    /* DEX  */
        case (0xCA<<3)|0: _SA(c->PC);break;
        case (0xCA<<3)|1: _FETCH();c->X--;_NZ(c->X);_ID();break;
        case (0xCA<<3)|2: assert(false);break;
        case (0xCA<<3)|3: assert(false);break;
        case (0xCA<<3)|4: assert(false);break;
        case (0xCA<<3)|5: assert(false);break;
        case (0xCA<<3)|6: assert(false);break;
        case (0xCA<<3)|7: assert(false);break;
    /* SBX # (undoc) */
        case (0xCB<<3)|0: _SA(c->PC++);break;
        case (0xCB<<3)|1: _FETCH();_m6502_sbx(c, _GD());break;
        case (0xCB<<3)|2: assert(false);break;
        case (0xCB<<3)|3: assert(false);break;
        case (0xCB<<3)|4: assert(false);break;
        case (0xCB<<3)|5: assert(false);break;
        case (0xCB<<3)|6: assert(false);break;
        case (0xCB<<3)|7: assert(false);break;
    /* CPY abs */
        case (0xCC<<3)|0: _SA(c->PC++);break;
        case (0xCC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xCC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCC<<3)|3: _FETCH();_m6502_cmp(c, c->Y, _GD());break;
        case (0xCC<<3)|4: assert(false);break;
        case (0xCC<<3)|5: assert(false);break;
        case (0xCC<<3)|6: assert(false);break;
        case (0xCC<<3)|7: assert(false);break;
    /* CMP abs */
        case (0xCD<<3)|0: _SA(c->PC++);break;
        case (0xCD<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xCD<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCD<<3)|3: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xCD<<3)|4: assert(false);break;
        case (0xCD<<3)|5: assert(false);break;
        case (0xCD<<3)|6: assert(false);break;
        case (0xCD<<3)|7: assert(false);break;
    /* DEC abs */
        case (0xCE<<3)|0: _SA(c->PC++);break;
        case (0xCE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xCE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCE<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xCE<<3)|4: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xCE<<3)|5: _FETCH();_ID();break;
        case (0xCE<<3)|6: assert(false);break;
        case (0xCE<<3)|7: assert(false);break;
    /* DCP abs (undoc) */
        case (0xCF<<3)|0: _SA(c->PC++);break;
        case (0xCF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xCF<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCF<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xCF<<3)|4: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xCF<<3)|5: _FETCH();_ID();break;
        case (0xCF<<3)|6: assert(false);break;
        case (0xCF<<3)|7: assert(false);break;
    /* BNE # */
        case (0xD0<<3)|0: _SA(c->PC++);break;
        case (0xD0<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x0){_FETCH();}else{_SA(c->PC);};break;
        case (0xD0<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0xD0<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0xD0<<3)|4: assert(false);break;
        case (0xD0<<3)|5: assert(false);break;
        case (0xD0<<3)|6: assert(false);break;
        case (0xD0<<3)|7: assert(false);break;
    /* CMP (zp),Y */
        case (0xD1<<3)|0: _SA(c->PC++);break;
        case (0xD1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD1<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xD1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xD1<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xD1<<3)|5: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xD1<<3)|6: assert(false);break;
        case (0xD1<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0xD2<<3)|0: _SA(c->PC);break;
        case (0xD2<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0xD2<<3)|2: assert(false);break;
        case (0xD2<<3)|3: assert(false);break;
        case (0xD2<<3)|4: assert(false);break;
        case (0xD2<<3)|5: assert(false);break;
        case (0xD2<<3)|6: assert(false);break;
        case (0xD2<<3)|7: assert(false);break;
    /* DCP (zp),Y (undoc) */
        case (0xD3<<3)|0: _SA(c->PC++);break;
        case (0xD3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD3<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xD3<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0xD3<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xD3<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xD3<<3)|6: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xD3<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0xD4<<3)|0: _SA(c->PC++);break;
        case (0xD4<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD4<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xD4<<3)|3: _FETCH();_ID();break;
        case (0xD4<<3)|4: assert(false);break;
        case (0xD4<<3)|5: assert(false);break;
        case (0xD4<<3)|6: assert(false);break;
        case (0xD4<<3)|7: assert(false);break;
    /* CMP zp,X */
        case (0xD5<<3)|0: _SA(c->PC++);break;
        case (0xD5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD5<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xD5<<3)|3: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xD5<<3)|4: assert(false);break;
        case (0xD5<<3)|5: assert(false);break;
        case (0xD5<<3)|6: assert(false);break;
        case (0xD5<<3)|7: assert(false);break;
    /* DEC zp,X */
        case (0xD6<<3)|0: _SA(c->PC++);break;
        case (0xD6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD6<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xD6<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xD6<<3)|4: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xD6<<3)|5: _FETCH();_ID();break;
        case (0xD6<<3)|6: assert(false);break;
        case (0xD6<<3)|7: assert(false);break;
    /* DCP zp,X (undoc) */
        case (0xD7<<3)|0: _SA(c->PC++);break;
        case (0xD7<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD7<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xD7<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xD7<<3)|4: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xD7<<3)|5: _FETCH();_ID();break;
        case (0xD7<<3)|6: assert(false);break;
        case (0xD7<<3)|7: assert(false);break;
    /* CLD  */
        case (0xD8<<3)|0: _SA(c->PC);break;
        case (0xD8<<3)|1: _FETCH();c->P&=~0x8;_ID();break;
        case (0xD8<<3)|2: assert(false);break;
        case (0xD8<<3)|3: assert(false);break;
        case (0xD8<<3)|4: assert(false);break;
        case (0xD8<<3)|5: assert(false);break;
        case (0xD8<<3)|6: assert(false);break;
        case (0xD8<<3)|7: assert(false);break;
    /* CMP abs,Y */
        case (0xD9<<3)|0: _SA(c->PC++);break;
        case (0xD9<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xD9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xD9<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xD9<<3)|4: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xD9<<3)|5: assert(false);break;
        case (0xD9<<3)|6: assert(false);break;
        case (0xD9<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0xDA<<3)|0: _SA(c->PC);break;
        case (0xDA<<3)|1: _FETCH();_ID();break;
        case (0xDA<<3)|2: assert(false);break;
        case (0xDA<<3)|3: assert(false);break;
        case (0xDA<<3)|4: assert(false);break;
        case (0xDA<<3)|5: assert(false);break;
        case (0xDA<<3)|6: assert(false);break;
        case (0xDA<<3)|7: assert(false);break;
    /* DCP abs,Y (undoc) */
        case (0xDB<<3)|0: _SA(c->PC++);break;
        case (0xDB<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xDB<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0xDB<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xDB<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xDB<<3)|5: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xDB<<3)|6: _FETCH();_ID();break;
        case (0xDB<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0xDC<<3)|0: _SA(c->PC++);break;
        case (0xDC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xDC<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xDC<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xDC<<3)|4: _FETCH();_ID();break;
        case (0xDC<<3)|5: assert(false);break;
        case (0xDC<<3)|6: assert(false);break;
        case (0xDC<<3)|7: assert(false);break;
    /* CMP abs,X */
        case (0xDD<<3)|0: _SA(c->PC++);break;
        case (0xDD<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xDD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xDD<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xDD<<3)|4: _FETCH();_m6502_cmp(c, c->A, _GD());break;
        case (0xDD<<3)|5: assert(false);break;
        case (0xDD<<3)|6: assert(false);break;
        case (0xDD<<3)|7: assert(false);break;
    /* DEC abs,X */
        case (0xDE<<3)|0: _SA(c->PC++);break;
        case (0xDE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xDE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0xDE<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xDE<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xDE<<3)|5: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xDE<<3)|6: _FETCH();_ID();break;
        case (0xDE<<3)|7: assert(false);break;
    /* DCP abs,X (undoc) */
        case (0xDF<<3)|0: _SA(c->PC++);break;
        case (0xDF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xDF<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0xDF<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xDF<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xDF<<3)|5: _WR();c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_ID();break;
        case (0xDF<<3)|6: _FETCH();_ID();break;
        case (0xDF<<3)|7: assert(false);break;
    /* CPX # */
        case (0xE0<<3)|0: _SA(c->PC++);break;
        case (0xE0<<3)|1: _FETCH();_m6502_cmp(c, c->X, _GD());break;
        case (0xE0<<3)|2: assert(false);break;
        case (0xE0<<3)|3: assert(false);break;
        case (0xE0<<3)|4: assert(false);break;
        case (0xE0<<3)|5: assert(false);break;
        case (0xE0<<3)|6: assert(false);break;
        case (0xE0<<3)|7: assert(false);break;
    /* SBC (zp,X) */
        case (0xE1<<3)|0: _SA(c->PC++);break;
        case (0xE1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xE1<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xE1<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xE1<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xE1<<3)|5: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xE1<<3)|6: assert(false);break;
        case (0xE1<<3)|7: assert(false);break;
    /* NOP # (undoc) */
        case (0xE2<<3)|0: _SA(c->PC++);break;
        case (0xE2<<3)|1: _FETCH();_ID();break;
        case (0xE2<<3)|2: assert(false);break;
        case (0xE2<<3)|3: assert(false);break;
        case (0xE2<<3)|4: assert(false);break;
        case (0xE2<<3)|5: assert(false);break;
        case (0xE2<<3)|6: assert(false);break;
        case (0xE2<<3)|7: assert(false);break;
    /* ISB (zp,X) (undoc) */
        case (0xE3<<3)|0: _SA(c->PC++);break;
        case (0xE3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xE3<<3)|2: c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);_ID();break;
        case (0xE3<<3)|3: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xE3<<3)|4: _SA((_GD()<<8)|c->AD);break;
        case (0xE3<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xE3<<3)|6: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xE3<<3)|7: _FETCH();_ID();break;
    /* CPX zp */
        case (0xE4<<3)|0: _SA(c->PC++);break;
        case (0xE4<<3)|1: _SA(_GD());break;
        case (0xE4<<3)|2: _FETCH();_m6502_cmp(c, c->X, _GD());break;
        case (0xE4<<3)|3: assert(false);break;
        case (0xE4<<3)|4: assert(false);break;
        case (0xE4<<3)|5: assert(false);break;
        case (0xE4<<3)|6: assert(false);break;
        case (0xE4<<3)|7: assert(false);break;
    /* SBC zp */
        case (0xE5<<3)|0: _SA(c->PC++);break;
        case (0xE5<<3)|1: _SA(_GD());break;
        case (0xE5<<3)|2: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xE5<<3)|3: assert(false);break;
        case (0xE5<<3)|4: assert(false);break;
        case (0xE5<<3)|5: assert(false);break;
        case (0xE5<<3)|6: assert(false);break;
        case (0xE5<<3)|7: assert(false);break;
    /* INC zp */
        case (0xE6<<3)|0: _SA(c->PC++);break;
        case (0xE6<<3)|1: _SA(_GD());break;
        case (0xE6<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xE6<<3)|3: _WR();c->AD++;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xE6<<3)|4: _FETCH();_ID();break;
        case (0xE6<<3)|5: assert(false);break;
        case (0xE6<<3)|6: assert(false);break;
        case (0xE6<<3)|7: assert(false);break;
    /* ISB zp (undoc) */
        case (0xE7<<3)|0: _SA(c->PC++);break;
        case (0xE7<<3)|1: _SA(_GD());break;
        case (0xE7<<3)|2: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xE7<<3)|3: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xE7<<3)|4: _FETCH();_ID();break;
        case (0xE7<<3)|5: assert(false);break;
        case (0xE7<<3)|6: assert(false);break;
        case (0xE7<<3)|7: assert(false);break;
    /* INX  */
        case (0xE8<<3)|0: _SA(c->PC);break;
        case (0xE8<<3)|1: _FETCH();c->X++;_NZ(c->X);_ID();break;
        case (0xE8<<3)|2: assert(false);break;
        case (0xE8<<3)|3: assert(false);break;
        case (0xE8<<3)|4: assert(false);break;
        case (0xE8<<3)|5: assert(false);break;
        case (0xE8<<3)|6: assert(false);break;
        case (0xE8<<3)|7: assert(false);break;
    /* SBC # */
        case (0xE9<<3)|0: _SA(c->PC++);break;
        case (0xE9<<3)|1: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xE9<<3)|2: assert(false);break;
        case (0xE9<<3)|3: assert(false);break;
        case (0xE9<<3)|4: assert(false);break;
        case (0xE9<<3)|5: assert(false);break;
        case (0xE9<<3)|6: assert(false);break;
        case (0xE9<<3)|7: assert(false);break;
    /* NOP  */
        case (0xEA<<3)|0: _SA(c->PC);break;
        case (0xEA<<3)|1: _FETCH();_ID();break;
        case (0xEA<<3)|2: assert(false);break;
        case (0xEA<<3)|3: assert(false);break;
        case (0xEA<<3)|4: assert(false);break;
        case (0xEA<<3)|5: assert(false);break;
        case (0xEA<<3)|6: assert(false);break;
        case (0xEA<<3)|7: assert(false);break;
    /* SBC # (undoc) */
        case (0xEB<<3)|0: _SA(c->PC++);break;
        case (0xEB<<3)|1: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xEB<<3)|2: assert(false);break;
        case (0xEB<<3)|3: assert(false);break;
        case (0xEB<<3)|4: assert(false);break;
        case (0xEB<<3)|5: assert(false);break;
        case (0xEB<<3)|6: assert(false);break;
        case (0xEB<<3)|7: assert(false);break;
    /* CPX abs */
        case (0xEC<<3)|0: _SA(c->PC++);break;
        case (0xEC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xEC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xEC<<3)|3: _FETCH();_m6502_cmp(c, c->X, _GD());break;
        case (0xEC<<3)|4: assert(false);break;
        case (0xEC<<3)|5: assert(false);break;
        case (0xEC<<3)|6: assert(false);break;
        case (0xEC<<3)|7: assert(false);break;
    /* SBC abs */
        case (0xED<<3)|0: _SA(c->PC++);break;
        case (0xED<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xED<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xED<<3)|3: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xED<<3)|4: assert(false);break;
        case (0xED<<3)|5: assert(false);break;
        case (0xED<<3)|6: assert(false);break;
        case (0xED<<3)|7: assert(false);break;
    /* INC abs */
        case (0xEE<<3)|0: _SA(c->PC++);break;
        case (0xEE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xEE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xEE<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xEE<<3)|4: _WR();c->AD++;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xEE<<3)|5: _FETCH();_ID();break;
        case (0xEE<<3)|6: assert(false);break;
        case (0xEE<<3)|7: assert(false);break;
    /* ISB abs (undoc) */
        case (0xEF<<3)|0: _SA(c->PC++);break;
        case (0xEF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xEF<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xEF<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xEF<<3)|4: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xEF<<3)|5: _FETCH();_ID();break;
        case (0xEF<<3)|6: assert(false);break;
        case (0xEF<<3)|7: assert(false);break;
    /* BEQ # */
        case (0xF0<<3)|0: _SA(c->PC++);break;
        case (0xF0<<3)|1: c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x2){_FETCH();}else{_SA(c->PC);};break;
        case (0xF0<<3)|2: if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->int_pip>>=1;_FETCH();}else{_SA((c->PC&0xFF00)|(c->AD&0x00FF));};_ID();break;
        case (0xF0<<3)|3: c->PC=c->AD;_FETCH();_ID();break;
        case (0xF0<<3)|4: assert(false);break;
        case (0xF0<<3)|5: assert(false);break;
        case (0xF0<<3)|6: assert(false);break;
        case (0xF0<<3)|7: assert(false);break;
    /* SBC (zp),Y */
        case (0xF1<<3)|0: _SA(c->PC++);break;
        case (0xF1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF1<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xF1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xF1<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xF1<<3)|5: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xF1<<3)|6: assert(false);break;
        case (0xF1<<3)|7: assert(false);break;
    /* JAM INVALID (undoc) */
        case (0xF2<<3)|0: _SA(c->PC);break;
        case (0xF2<<3)|1: _SAD(0xFFFF,0xFF);c->IR--;_ID();break;
        case (0xF2<<3)|2: assert(false);break;
        case (0xF2<<3)|3: assert(false);break;
        case (0xF2<<3)|4: assert(false);break;
        case (0xF2<<3)|5: assert(false);break;
        case (0xF2<<3)|6: assert(false);break;
        case (0xF2<<3)|7: assert(false);break;
    /* ISB (zp),Y (undoc) */
        case (0xF3<<3)|0: _SA(c->PC++);break;
        case (0xF3<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF3<<3)|2: _SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xF3<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0xF3<<3)|4: _SA(c->AD+c->Y);_ID();break;
        case (0xF3<<3)|5: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xF3<<3)|6: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xF3<<3)|7: _FETCH();_ID();break;
    /* NOP zp,X (undoc) */
        case (0xF4<<3)|0: _SA(c->PC++);break;
        case (0xF4<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF4<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xF4<<3)|3: _FETCH();_ID();break;
        case (0xF4<<3)|4: assert(false);break;
        case (0xF4<<3)|5: assert(false);break;
        case (0xF4<<3)|6: assert(false);break;
        case (0xF4<<3)|7: assert(false);break;
    /* SBC zp,X */
        case (0xF5<<3)|0: _SA(c->PC++);break;
        case (0xF5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF5<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xF5<<3)|3: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xF5<<3)|4: assert(false);break;
        case (0xF5<<3)|5: assert(false);break;
        case (0xF5<<3)|6: assert(false);break;
        case (0xF5<<3)|7: assert(false);break;
    /* INC zp,X */
        case (0xF6<<3)|0: _SA(c->PC++);break;
        case (0xF6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF6<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xF6<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xF6<<3)|4: _WR();c->AD++;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xF6<<3)|5: _FETCH();_ID();break;
        case (0xF6<<3)|6: assert(false);break;
        case (0xF6<<3)|7: assert(false);break;
    /* ISB zp,X (undoc) */
        case (0xF7<<3)|0: _SA(c->PC++);break;
        case (0xF7<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF7<<3)|2: _SA((c->AD+c->X)&0x00FF);_ID();break;
        case (0xF7<<3)|3: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xF7<<3)|4: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xF7<<3)|5: _FETCH();_ID();break;
        case (0xF7<<3)|6: assert(false);break;
        case (0xF7<<3)|7: assert(false);break;
    /* SED  */
        case (0xF8<<3)|0: _SA(c->PC);break;
        case (0xF8<<3)|1: _FETCH();c->P|=0x8;_ID();break;
        case (0xF8<<3)|2: assert(false);break;
        case (0xF8<<3)|3: assert(false);break;
        case (0xF8<<3)|4: assert(false);break;
        case (0xF8<<3)|5: assert(false);break;
        case (0xF8<<3)|6: assert(false);break;
        case (0xF8<<3)|7: assert(false);break;
    /* SBC abs,Y */
        case (0xF9<<3)|0: _SA(c->PC++);break;
        case (0xF9<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xF9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;break;
        case (0xF9<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xF9<<3)|4: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xF9<<3)|5: assert(false);break;
        case (0xF9<<3)|6: assert(false);break;
        case (0xF9<<3)|7: assert(false);break;
    /* NOP  (undoc) */
        case (0xFA<<3)|0: _SA(c->PC);break;
        case (0xFA<<3)|1: _FETCH();_ID();break;
        case (0xFA<<3)|2: assert(false);break;
        case (0xFA<<3)|3: assert(false);break;
        case (0xFA<<3)|4: assert(false);break;
        case (0xFA<<3)|5: assert(false);break;
        case (0xFA<<3)|6: assert(false);break;
        case (0xFA<<3)|7: assert(false);break;
    /* ISB abs,Y (undoc) */
        case (0xFB<<3)|0: _SA(c->PC++);break;
        case (0xFB<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xFB<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));break;
        case (0xFB<<3)|3: _SA(c->AD+c->Y);_ID();break;
        case (0xFB<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xFB<<3)|5: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xFB<<3)|6: _FETCH();_ID();break;
        case (0xFB<<3)|7: assert(false);break;
    /* NOP abs,X (undoc) */
        case (0xFC<<3)|0: _SA(c->PC++);break;
        case (0xFC<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xFC<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xFC<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xFC<<3)|4: _FETCH();_ID();break;
        case (0xFC<<3)|5: assert(false);break;
        case (0xFC<<3)|6: assert(false);break;
        case (0xFC<<3)|7: assert(false);break;
    /* SBC abs,X */
        case (0xFD<<3)|0: _SA(c->PC++);break;
        case (0xFD<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xFD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;break;
        case (0xFD<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xFD<<3)|4: _FETCH();_m6502_sbc(c,_GD());break;
        case (0xFD<<3)|5: assert(false);break;
        case (0xFD<<3)|6: assert(false);break;
        case (0xFD<<3)|7: assert(false);break;
    /* INC abs,X */
        case (0xFE<<3)|0: _SA(c->PC++);break;
        case (0xFE<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xFE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0xFE<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xFE<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xFE<<3)|5: _WR();c->AD++;_NZ(c->AD);_SD(c->AD);_ID();break;
        case (0xFE<<3)|6: _FETCH();_ID();break;
        case (0xFE<<3)|7: assert(false);break;
    /* ISB abs,X (undoc) */
        case (0xFF<<3)|0: _SA(c->PC++);break;
        case (0xFF<<3)|1: _SA(c->PC++);c->AD=_GD();break;
        case (0xFF<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));break;
        case (0xFF<<3)|3: _SA(c->AD+c->X);_ID();break;
        case (0xFF<<3)|4: _WR();c->AD=_GD();_SD(c->AD);break;
        case (0xFF<<3)|5: _WR();c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_ID();break;
        case (0xFF<<3)|6: _FETCH();_ID();break;
        case (0xFF<<3)|7: assert(false);break;

    }
    c->PINS = pins;
    return pins;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#undef _SA
#undef _SAD
#undef _FETCH
#undef _SD
#undef _GD
#undef _ON
#undef _OFF
#undef _RD
#undef _WR
#undef _NZ
#undef _ID
#undef _IA
#endif /* CHIPS_IMPL */
