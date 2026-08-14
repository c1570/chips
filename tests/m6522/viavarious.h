/*
    Shared cycle-timed harness for the m6522 viavarious test ports.

    Replicates the 1541 drive-side code from the VICE test framework
    (vice-testprogs/drive/viavarious/common.asm + framework-drive.asm)
    cycle-by-cycle against m6522.h, without any C64 or drive emulation.
    One m6522_tick() call == one drive clock cycle == one 6502 bus
    cycle. VIA register accesses happen on the last cycle of the
    emulated lda/sta instruction, just like on a real 6502 bus.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "chips/m6522.h"

/*--- cycle-timed bus access ----------------------------------------------*/

static m6522_t via;

/*
    Static input levels on the 1541 VIA1 port B (measured in the
    via10 reference data): PB0..PB4 are the serial bus lines with
    pull-ups (released -> high), PB5/PB6 are the device number
    jumpers (grounded -> low). PB6 low and static means T2 configured
    to count PB6 is effectively stopped, PA/CA/CB input pins are held
    low (no port A/B reads or handshake edges in these tests).
*/
#define BASE_PINS (0x1FULL << 56)

/* n bus cycles with the VIA deselected */
static void idle(uint32_t n) {
    while (n-- > 0) {
        (void)m6522_tick(&via, BASE_PINS);
    }
}

/* one bus cycle reading a VIA register (CS1=1, CS2=0, RW=1) */
static uint8_t rd_cycle(uint8_t reg) {
    uint64_t pins = m6522_tick(&via, BASE_PINS | M6522_CS1 | M6522_RW | (reg & M6522_RS_PINS));
    return M6522_GET_DATA(pins);
}

/* one bus cycle writing a VIA register (CS1=1, CS2=0, RW=0) */
static void wr_cycle(uint8_t reg, uint8_t data) {
    (void)m6522_tick(&via, BASE_PINS | M6522_CS1 | (reg & M6522_RS_PINS) | ((uint64_t)data << 16));
}

/* 6502 cycle counts for the instructions used by the original code */
enum {
    CY_SEI       = 2,
    CY_LDA_IMM   = 2,
    CY_LDX_IMM   = 2,
    CY_LDY_IMM   = 2,
    CY_TYA       = 2,
    CY_INX       = 2,
    CY_INY       = 2,
    CY_DEY       = 2,
    CY_EOR_IMM   = 2,
    CY_LDA_ABS   = 4,   /* register is read on the last of the 4 cycles */
    CY_STA_ABS   = 4,   /* register is written on the last of the 4 cycles */
    CY_STA_ABSX  = 5,   /* sta DTMP,x */
    CY_STA_ABSY  = 5,   /* sta DTMP,y */
    CY_BNE_TAKEN = 3,
    CY_BNE_FALL  = 2,
    CY_JSR       = 6,
    CY_RTS       = 6
};

/* 'sta $18xx': 3 idle cycles, register write on the 4th cycle */
static void i_sta_abs(uint8_t reg, uint8_t val) {
    idle(CY_LDA_ABS - 1);
    wr_cycle(reg, val);
}

/* 'lda $18xx': 3 idle cycles, register read on the 4th cycle */
static uint8_t i_lda_abs(uint8_t reg) {
    idle(CY_LDA_ABS - 1);
    return rd_cycle(reg);
}

/*--- drive-side prologue, common.asm drvstart .. jsr ddotest --------------*/

static void drivecode_prologue(void) {
    /* drvstart: sei ; jsr .setdefaults */
    idle(CY_SEI);
    idle(CY_JSR);

    /* --- .setdefaults --- */
    idle(CY_LDA_IMM);                       /* lda #%00100000 */
    i_sta_abs(M6522_REG_ACR, 0x20);         /* T2 counts PB6 (= stopped) */
    idle(CY_LDA_IMM);                       /* lda #0 */
    i_sta_abs(M6522_REG_PCR, 0x00);
    idle(CY_LDA_IMM);                       /* lda #$a5 */
    i_sta_abs(M6522_REG_SR, 0xA5);          /* serial shift register */
    idle(CY_LDA_IMM);                       /* lda #$7f */
    i_sta_abs(M6522_REG_IER, 0x7F);         /* disable all IRQs */
    uint8_t ifr = i_lda_abs(M6522_REG_IFR); /* acknowledge pending IRQs */
    i_sta_abs(M6522_REG_IFR, ifr);          /*   (write back read value)  */
    idle(CY_LDY_IMM + CY_TYA);              /* ldy #0 ; tya (A = 0) */
    for (int i = 0; i < 256; i++) {         /* write $00 to $1804..$1809 */
        i_sta_abs(M6522_REG_T1CL, 0);
        i_sta_abs(M6522_REG_T1CH, 0);
        i_sta_abs(M6522_REG_T1LL, 0);
        i_sta_abs(M6522_REG_T1LH, 0);
        i_sta_abs(M6522_REG_T2CL, 0);
        i_sta_abs(M6522_REG_T2CH, 0);
        idle((i == 255) ? (CY_DEY + CY_BNE_FALL)
                        : (CY_DEY + CY_BNE_TAKEN));
    }
    idle(CY_RTS);

    /* --- jsr snd_init (framework-drive.asm) --- */
    idle(CY_JSR);
    idle(CY_LDA_IMM);                       /* lda #%01111010 */
    i_sta_abs(M6522_REG_DDRB, 0x7A);        /* serial lines output */
    idle(CY_LDA_IMM);                       /* lda #0 */
    i_sta_abs(M6522_REG_RB, 0x00);          /* CLOCK = 0, DATA = 0 */
    idle(CY_RTS);

    /* --- clear the DTMP result buffer --- */
    idle(CY_LDA_IMM + CY_LDY_IMM);          /* lda #0 ; ldy #0 */
    for (int i = 0; i < 256; i++) {
        idle(CY_STA_ABSY);                  /* sta DTMP,y */
        idle((i == 255) ? (CY_INY + CY_BNE_FALL)
                        : (CY_INY + CY_BNE_TAKEN));
    }
    idle(CY_JSR);                           /* jsr ddotest -> test code */
}

/*--- generic sub test runner ----------------------------------------------*/

/* loop shapes used by the viavarious sub tests */
typedef enum {
    LOOP_READ,             /* 14 cycles: lda reg; sta DTMP,x; inx; bne */
    LOOP_READ_ACR_TOGGLE,  /* 24 cycles: + lda ACR; eor #$40; sta ACR (via4) */
    LOOP_READ_ACR_TOGGLE2, /* 24 cycles: + lda ACR; eor #$20; sta ACR (via9) */
    LOOP_STX_READ,         /* 18 cycles: stx wr_reg; lda reg; sta DTMP,x; inx; bne (via5 a..l) */
    LOOP_STX_ACR0_READ,    /* 24 cycles: stx wr_reg; lda #0; sta ACR; lda reg; ... (via5 m..r) */
} loop_kind_t;

typedef struct {
    const char* letter;
    const char* desc;
    int setup_reg;      /* -1: none, else 'lda #setup_val; sta setup_reg' before the loop */
    uint8_t setup_val;
    int acr;            /* -1: none, else 'lda #acr; sta $180b' after the setup write */
    uint8_t read_reg;   /* register read 256 times in the loop */
    loop_kind_t loop;
    int wr_reg;         /* LOOP_STX_*: register written with the loop counter each iteration */
} subtest_t;

/*
    All viavarious sub tests share this shape:

      [lda #val]               (optional setup write)
      [sta $18xx]
      [lda #%acr]              (optional ACR write)
      [sta $180b]
      ldx #0
    .t1b:
      [stx $18xx]              (optional, via5: write loop counter to a timer register)
      [lda #%00000000]         (optional, via5 m..r: ACR=0, T2 counts clock)
      [sta $180b]
      lda $18xx                ; read on the 4th cycle of the instruction
      sta DTMP,x
      [lda $180b]              (optional, via4: toggle T1 continuous mode)
      [eor #%01000000]
      [sta $180b]
      inx
      bne .t1b
      rts

    Loop iteration lengths: LOOP_READ 14 cycles (lda abs 4 + sta abs,x 5
    + inx 2 + bne taken 3), LOOP_READ_ACR_TOGGLE 24, LOOP_STX_READ 18
    (+ stx abs 4), LOOP_STX_ACR0_READ 24 (+ stx abs 4 + lda imm 2 +
    sta abs 4).

    A fresh drive-side run (init + prologue) is done for every sub test,
    matching the original framework which resets the drive between tests.
    The trailing instructions of the last iteration happen after the
    final read and cannot influence the buffer.
*/
static void run_subtest(const subtest_t* t, uint8_t* buf) {
    m6522_init(&via);
    drivecode_prologue();
    if (t->setup_reg >= 0) {
        idle(CY_LDA_IMM);
        i_sta_abs((uint8_t)t->setup_reg, t->setup_val);
    }
    if (t->acr >= 0) {
        idle(CY_LDA_IMM);                   /* lda #%acr */
        i_sta_abs(M6522_REG_ACR, (uint8_t)t->acr);  /* sta $180b */
    }
    idle(CY_LDX_IMM);                       /* ldx #0 */
    for (int i = 0; i < 256; i++) {
        if (t->loop == LOOP_STX_READ || t->loop == LOOP_STX_ACR0_READ) {
            idle(CY_STA_ABS - 1);                       /* stx $18xx (4 cycles) */
            wr_cycle((uint8_t)t->wr_reg, (uint8_t)i);   /* write on the 4th cycle */
            if (t->loop == LOOP_STX_ACR0_READ) {
                idle(CY_LDA_IMM);                       /* lda #%00000000 */
                i_sta_abs(M6522_REG_ACR, 0x00);         /* sta $180b */
            }
        }
        buf[i] = i_lda_abs(t->read_reg);    /* lda $18xx */
        if (i < 255) {
            idle(CY_STA_ABSX);                          /* sta DTMP,x */
            if (t->loop == LOOP_READ_ACR_TOGGLE || t->loop == LOOP_READ_ACR_TOGGLE2) {
                uint8_t mask = (t->loop == LOOP_READ_ACR_TOGGLE) ? 0x40 : 0x20;
                uint8_t acr = i_lda_abs(M6522_REG_ACR); /* lda $180b */
                idle(CY_EOR_IMM);                       /* eor #mask */
                i_sta_abs(M6522_REG_ACR, acr ^ mask);   /* sta $180b */
            }
            idle(CY_INX + CY_BNE_TAKEN);
        }
    }
}

/*--- comparison and reporting ---------------------------------------------*/

static bool report(const char* letter, const char* desc, const uint8_t* buf, const uint8_t* ref) {
    int diffs = 0, first = -1, shown = 0;
    for (int i = 0; i < 256; i++) {
        if (buf[i] != ref[i]) {
            if (first < 0) first = i;
            diffs++;
        }
    }
    if (diffs == 0) {
        printf("  [%s] PASS  %s\n", letter, desc);
        return true;
    }
    printf("  [%s] FAIL  %s\n", letter, desc);
    for (int i = first; i < 256 && shown < 8; i++) {
        if (buf[i] != ref[i]) {
            printf("         @%3d: got %02X want %02X\n", i, buf[i], ref[i]);
            shown++;
        }
    }
    if (diffs > shown) {
        printf("         ... %d of 256 bytes differ\n", diffs);
    }
    return false;
}
