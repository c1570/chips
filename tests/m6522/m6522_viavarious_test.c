/*
    m6522 VIA tests, ported from the VICE drive test programs
    vice-testprogs/drive/viavarious/via1.asm .. via5.asm, via3a.asm,
    via9.asm .. via14.asm, via20.asm and via21.asm.

    Originals (+ common.asm, framework-drive.asm) run on a real C64 +
    1541 drive and compare against data recorded from a real 1541C
    (via1ref.bin .. via5ref.bin, via3aref.bin, via9ref.bin,
    via10ref.bin .. via14ref.bin, via20ref.bin, via21ref.bin).

    The 1541 drive-side code (6510 CPU @ 1 MHz, VIA1 at $1800) is
    replicated here cycle-by-cycle against m6522.h directly, without
    any C64 or drive emulation (see viavarious.h).

    Cycle timing matters: the plain read loop takes 14 cycles per
    iteration (lda abs: 4, sta abs,x: 5, inx: 2, bne taken: 3), the
    via4/via9 loops with the ACR toggle 24 cycles, the via5 loops 18
    cycles (with an 'stx $18xx' register write) and 24 cycles (stx +
    ACR=0). The reference data encodes exactly this.

    via1 (a..h), each sub test fills a 256 byte buffer:
      - before each sub test the '.setdefaults' init from common.asm
        runs: ACR=$20 (T2 counts PB6 -> stopped, PB6 is static on the
        1541), PCR=0, SR=$A5, IER=$7F (IRQs disabled), pending IRQs
        acknowledged, then a 256x loop writing $00 to $1804..$1809
        (all T1/T2 counters and latches end up as $0000)
      - a..f: read [T1CL|T1CH|T1LL|T1LH|T2CL|T2CH] 256 times
      - g,h:  write ACR=0 (T2 now counts clock cycles), then read
        [T2CL|T2CH] 256 times (first read lands at -5 = $FB, then
        -14 per read)

    via2 (a..l), each sub test fills a 256 byte buffer:
      - same '.setdefaults' init, plus one setup write:
      - a..d: T1CL=1, read [T1CL|T1CH|T1LL|T1LH]
          T1 latch becomes $0001; writing T1CL does NOT load the
          counter, so T1 keeps free-running, reloading from the new
          latch on the next underflow (counter cycles 1,0,$FF -> the
          read loop samples every 14 ticks and aliases through that
          3 tick period: 00 01 ff ...)
      - e..h: T1CH=1, read [T1CL|T1CH|T1LL|T1LH]
          writing T1CH loads the counter from the latch ($0100), T1
          counts down from there; latch registers read back $0000/$0100
      - i..l: T2CL=1 / T2CH=1 + ACR=0 (T2 counts clock), read
          [T2CL|T2CH]
          writing T2CL only sets the latch (counter stays $0000 and
          counts down from the wrap), writing T2CH additionally loads
          the counter ($0100); either way the first read lands at -5
          ($FB), then -14 per read

    via3 (a..l), each sub test fills a 256 byte buffer:
      - same '.setdefaults' init, then one timer setup write, one ACR
        write, then the loop reads IFR ($180d, no read side effects):
      - a..d: T1CL=1, ACR=[$00|$40|$80|$C0], read IFR
          T1CL only sets the latch, the counter keeps oscillating
          around the old latch value 0 with the one-shot flag latch
          already consumed, so the T1 flag ($40) never fires on real
          hardware; ACR bit5 is 0 in all four values, so T2 switches
          to clock counting, its counter (at $0000) wraps immediately
          and latches the T2 flag ($20): all reads are $20
          NOTE: b and d (T1 continuous mode) currently FAIL on
          m6522.h: it sets the T1 IRQ flag at every counter underflow
          in continuous mode, so during the post-init 1,0,$FF counter
          oscillation the flag is set immediately (reads $60 instead
          of $20). Real silicon never sets the flag in this state;
          likely needs the FIXME'd continuous-mode counter handling in
          _m6522_write() (chips/m6522.h) to be resolved.
      - e..h: T1CH=1, ACR=[$00|$40|$80|$C0], read IFR
          T1CH=1 loads the counter ($0100), T1 underflows 257 ticks
          later and sets the T1 flag: the first 18 reads are $20, from
          read 18 on (read n happens 12+14n ticks after the T1CH
          write) it is $60; reading IFR does not clear it
      - i:    T2CL=1, ACR=$00: T2 wraps immediately after the mode
          switch, all reads $20
      - j:    T2CL=1, ACR=$20: T2 keeps counting PB6 (= stopped on the
          1541), no flag ever set, all reads $00
      - k:    T2CH=1, ACR=$00: T2 counter loaded with $0100, underflow
          after 257 ticks: $00 for the first 18 reads, then $20
      - l:    T2CH=1, ACR=$20: T2 stopped, all reads $00

    via4 (a..x), 24 sub tests: 4 ACR groups ($00/$40/$80/$C0), each
    with (T1CL=1 | T1CH=1) setup x (read T1CL | T1CH | IFR):
      - the loop reads the register and then toggles ACR bit 6 (T1
        one-shot <-> continuous) every iteration: 'lda $180b; eor
        #%01000000; sta $180b' => 24 cycle loop, first read 12 ticks
        after the setup write, then -24 per read
      - the reference data is identical for all four ACR groups: on
        real hardware the mode toggling does not influence the T1
        counter values or the IRQ flags at all
      - T1CL=1 groups (a,b,g,h,m,n,s,t): counter oscillates around the
        old latch 0 -> reads $00 (and $00 hi byte)
      - T1CH=1 + read T1CL (c,i,o,u): counter loaded $0100, first read
        at -11 ($F5), then -24 per read
      - T1CH=1 + read T1CH (d,j,p,v): hi byte $00, briefly $FF around
        the wraps
      - T1CL=1 + read IFR (e,k,q,w): $20 forever, the T1 flag never
          fires on real hardware (like via3 a..d)
          NOTE: these currently FAIL on m6522.h for the same reason as
          via3 b/d: m6522.h sets the T1 flag at every underflow while
          in continuous mode, so the first toggle into continuous sets
          it. e/q (start one-shot) read $20 once, then $60 from read 1
          on; k/w (start continuous) read $60 from read 0 on
      - T1CH=1 + read IFR (f,l,r,x): $20 for the first 11 reads (read
        n happens 12+24n ticks after the T1CH write), then $60 when T1
        underflows at +257

    Failures are reported but do not abort the run; the exit code is
    the total number of failed sub tests (0 == all green).

    Build:  cc -std=c11 -Wall -Wextra -I../.. -o m6522_viavarious_test m6522_viavarious_test.c
    Run:    ./m6522_viavarious_test
*/
#include <stdio.h>
#include <stdint.h>

#define CHIPS_IMPL
#include "viavarious.h"
#include "m6522_via1ref.h"
#include "m6522_via2ref.h"
#include "m6522_via3ref.h"
#include "m6522_via4ref.h"
#include "m6522_via3aref.h"
#include "m6522_via5ref.h"
#include "m6522_via9ref.h"
#include "m6522_via10ref.h"
#include "m6522_via11ref.h"
#include "m6522_via12ref.h"
#include "m6522_via13ref.h"
#include "m6522_via14ref.h"
#include "m6522_via20ref.h"
#include "m6522_via21ref.h"

/*--- via1: plain timer register reads -------------------------------------*/

#define VIA1_NUMTESTS 8

static const subtest_t via1_subtests[VIA1_NUMTESTS] = {
    { "a", "read Timer A lo       ($1804)", -1, 0, -1, M6522_REG_T1CL, LOOP_READ, -1 },
    { "b", "read Timer A hi       ($1805)", -1, 0, -1, M6522_REG_T1CH, LOOP_READ, -1 },
    { "c", "read Timer A latch lo ($1806)", -1, 0, -1, M6522_REG_T1LL, LOOP_READ, -1 },
    { "d", "read Timer A latch hi ($1807)", -1, 0, -1, M6522_REG_T1LH, LOOP_READ, -1 },
    { "e", "read Timer B lo       ($1808)", -1, 0, -1, M6522_REG_T2CL, LOOP_READ, -1 },
    { "f", "read Timer B hi       ($1809)", -1, 0, -1, M6522_REG_T2CH, LOOP_READ, -1 },
    { "g", "ACR=0, read Timer B lo ($1808)", -1, 0, 0x00, M6522_REG_T2CL, LOOP_READ, -1 },
    { "h", "ACR=0, read Timer B hi ($1809)", -1, 0, 0x00, M6522_REG_T2CH, LOOP_READ, -1 },
};

/*--- via2: timer register reads after a setup write -----------------------*/

#define VIA2_NUMTESTS 12

static const subtest_t via2_subtests[VIA2_NUMTESTS] = {
    { "a", "T1CL=1, read Timer A lo       ($1804)", M6522_REG_T1CL, 1, -1, M6522_REG_T1CL, LOOP_READ, -1 },
    { "b", "T1CL=1, read Timer A hi       ($1805)", M6522_REG_T1CL, 1, -1, M6522_REG_T1CH, LOOP_READ, -1 },
    { "c", "T1CL=1, read Timer A latch lo ($1806)", M6522_REG_T1CL, 1, -1, M6522_REG_T1LL, LOOP_READ, -1 },
    { "d", "T1CL=1, read Timer A latch hi ($1807)", M6522_REG_T1CL, 1, -1, M6522_REG_T1LH, LOOP_READ, -1 },
    { "e", "T1CH=1, read Timer A lo       ($1804)", M6522_REG_T1CH, 1, -1, M6522_REG_T1CL, LOOP_READ, -1 },
    { "f", "T1CH=1, read Timer A hi       ($1805)", M6522_REG_T1CH, 1, -1, M6522_REG_T1CH, LOOP_READ, -1 },
    { "g", "T1CH=1, read Timer A latch lo ($1806)", M6522_REG_T1CH, 1, -1, M6522_REG_T1LL, LOOP_READ, -1 },
    { "h", "T1CH=1, read Timer A latch hi ($1807)", M6522_REG_T1CH, 1, -1, M6522_REG_T1LH, LOOP_READ, -1 },
    { "i", "T2CL=1 + ACR=0, read Timer B lo ($1808)", M6522_REG_T2CL, 1, 0x00, M6522_REG_T2CL, LOOP_READ, -1 },
    { "j", "T2CL=1 + ACR=0, read Timer B hi ($1809)", M6522_REG_T2CL, 1, 0x00, M6522_REG_T2CH, LOOP_READ, -1 },
    { "k", "T2CH=1 + ACR=0, read Timer B lo ($1808)", M6522_REG_T2CH, 1, 0x00, M6522_REG_T2CL, LOOP_READ, -1 },
    { "l", "T2CH=1 + ACR=0, read Timer B hi ($1809)", M6522_REG_T2CH, 1, 0x00, M6522_REG_T2CH, LOOP_READ, -1 },
};

/*--- via3: IRQ flag reads ---------------------------------------------------*/

#define VIA3_NUMTESTS 12

static const subtest_t via3_subtests[VIA3_NUMTESTS] = {
    { "a", "T1CL=1, ACR=$00 (T1 one-shot), read IFR ($180d)",     M6522_REG_T1CL, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "b", "T1CL=1, ACR=$40 (T1 continuous), read IFR ($180d)",   M6522_REG_T1CL, 1, 0x40, M6522_REG_IFR, LOOP_READ, -1 },
    { "c", "T1CL=1, ACR=$80 (T1 one-shot, PB7), read IFR ($180d)", M6522_REG_T1CL, 1, 0x80, M6522_REG_IFR, LOOP_READ, -1 },
    { "d", "T1CL=1, ACR=$C0 (T1 contin., PB7), read IFR ($180d)", M6522_REG_T1CL, 1, 0xC0, M6522_REG_IFR, LOOP_READ, -1 },
    { "e", "T1CH=1, ACR=$00 (T1 one-shot), read IFR ($180d)",     M6522_REG_T1CH, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "f", "T1CH=1, ACR=$40 (T1 continuous), read IFR ($180d)",   M6522_REG_T1CH, 1, 0x40, M6522_REG_IFR, LOOP_READ, -1 },
    { "g", "T1CH=1, ACR=$80 (T1 one-shot, PB7), read IFR ($180d)", M6522_REG_T1CH, 1, 0x80, M6522_REG_IFR, LOOP_READ, -1 },
    { "h", "T1CH=1, ACR=$C0 (T1 contin., PB7), read IFR ($180d)", M6522_REG_T1CH, 1, 0xC0, M6522_REG_IFR, LOOP_READ, -1 },
    { "i", "T2CL=1, ACR=$00 (T2 clock), read IFR ($180d)",        M6522_REG_T2CL, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "j", "T2CL=1, ACR=$20 (T2 counts PB6), read IFR ($180d)",   M6522_REG_T2CL, 1, 0x20, M6522_REG_IFR, LOOP_READ, -1 },
    { "k", "T2CH=1, ACR=$00 (T2 clock), read IFR ($180d)",        M6522_REG_T2CH, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "l", "T2CH=1, ACR=$20 (T2 counts PB6), read IFR ($180d)",   M6522_REG_T2CH, 1, 0x20, M6522_REG_IFR, LOOP_READ, -1 },
};

/*--- via4: T1 reads while toggling T1 one-shot/continuous mode --------------*/

#define VIA4_NUMTESTS 24

/*
    Setup combinations, 4 ACR groups x (T1CL=1 | T1CH=1) x (read T1CL |
    T1CH | IFR). In the loop, ACR bit 6 is toggled every iteration
    (24 cycle loop, first read 12 ticks after the setup write, then
    -24 per read). The reference data is identical for all four ACR
    groups: on real hardware the mode toggling does not influence the
    counter values or the IRQ flags.
*/
static const subtest_t via4_subtests[VIA4_NUMTESTS] = {
    /* ACR=$00 group (starts one-shot, toggles $00<->$40) */
    { "a", "T1CL=1, ACR=$00, read T1CL", M6522_REG_T1CL, 1, 0x00, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "b", "T1CL=1, ACR=$00, read T1CH", M6522_REG_T1CL, 1, 0x00, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "c", "T1CH=1, ACR=$00, read T1CL", M6522_REG_T1CH, 1, 0x00, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "d", "T1CH=1, ACR=$00, read T1CH", M6522_REG_T1CH, 1, 0x00, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "e", "T1CL=1, ACR=$00, read IFR",  M6522_REG_T1CL, 1, 0x00, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    { "f", "T1CH=1, ACR=$00, read IFR",  M6522_REG_T1CH, 1, 0x00, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    /* ACR=$40 group (starts continuous, toggles $40<->$00) */
    { "g", "T1CL=1, ACR=$40, read T1CL", M6522_REG_T1CL, 1, 0x40, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "h", "T1CL=1, ACR=$40, read T1CH", M6522_REG_T1CL, 1, 0x40, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "i", "T1CH=1, ACR=$40, read T1CL", M6522_REG_T1CH, 1, 0x40, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "j", "T1CH=1, ACR=$40, read T1CH", M6522_REG_T1CH, 1, 0x40, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "k", "T1CL=1, ACR=$40, read IFR",  M6522_REG_T1CL, 1, 0x40, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    { "l", "T1CH=1, ACR=$40, read IFR",  M6522_REG_T1CH, 1, 0x40, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    /* ACR=$80 group (starts one-shot + PB7, toggles $80<->$C0) */
    { "m", "T1CL=1, ACR=$80, read T1CL", M6522_REG_T1CL, 1, 0x80, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "n", "T1CL=1, ACR=$80, read T1CH", M6522_REG_T1CL, 1, 0x80, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "o", "T1CH=1, ACR=$80, read T1CL", M6522_REG_T1CH, 1, 0x80, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "p", "T1CH=1, ACR=$80, read T1CH", M6522_REG_T1CH, 1, 0x80, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "q", "T1CL=1, ACR=$80, read IFR",  M6522_REG_T1CL, 1, 0x80, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    { "r", "T1CH=1, ACR=$80, read IFR",  M6522_REG_T1CH, 1, 0x80, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    /* ACR=$C0 group (starts continuous + PB7, toggles $C0<->$80) */
    { "s", "T1CL=1, ACR=$C0, read T1CL", M6522_REG_T1CL, 1, 0xC0, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "t", "T1CL=1, ACR=$C0, read T1CH", M6522_REG_T1CL, 1, 0xC0, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "u", "T1CH=1, ACR=$C0, read T1CL", M6522_REG_T1CH, 1, 0xC0, M6522_REG_T1CL, LOOP_READ_ACR_TOGGLE, -1 },
    { "v", "T1CH=1, ACR=$C0, read T1CH", M6522_REG_T1CH, 1, 0xC0, M6522_REG_T1CH, LOOP_READ_ACR_TOGGLE, -1 },
    { "w", "T1CL=1, ACR=$C0, read IFR",  M6522_REG_T1CL, 1, 0xC0, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
    { "x", "T1CH=1, ACR=$C0, read IFR",  M6522_REG_T1CH, 1, 0xC0, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE, -1 },
};

/*--- via5: timer registers written with the loop counter each iteration ----*/

#define VIA5_NUMTESTS 18

/*
    No setup writes; each loop iteration first writes the loop counter
    X (0..255) to a timer register ('stx $18xx', 18 cycle loop; the
    m..r groups additionally write ACR=0 every iteration so T2 counts
    clock, 24 cycle loop), then reads a register.

      - a..f: stx [T1CL|T1CH], read [T1CL|T1CH|IFR]
          T1CL only updates the latch lo while the counter keeps
          free-running around the (moving) latch -> aliasing patterns;
          T1CH loads the counter from the latch ($XX00) and clears the
          T1 flag every iteration: the read lands 3 decrements later
          ($XXFD); f shows the flag set once for X=0 (counter=0 wraps
          immediately) and cleared again by the next write
          NOTE: c and d currently FAIL on m6522.h: the read happens
          only 4 cycles after the stx T1CH reload, and m6522.h's
          counter is one decrement further than real hardware
          (reads $FC instead of $FD; for X=0 $00 instead of $FF) - a
          T1 load / count-restart timing deviation that only shows up
          at this tight spacing
      - g..l: stx [T1LL|T1LH], read [T1LL|T1LH|IFR]
          latch registers read straight back what was written (g/j are
          the identity ramp 0..255, h/i the untouched other byte $00);
          latch writes never load the counter or fire flags: k/l all $00
      - m..r: stx [T2CL|T2CH] + ACR=0 in the loop, read [T2CL|T2CH|IFR]
          m: T2CL only sets the latch, the counter (at $0000) starts
          counting at the first ACR=0 write; first read at -3 ($FD),
          then -24 per read (the ACR=0 write is inside the 24 cycle
          loop);
          o: T2CH loads the counter ($XX00) every iteration, the read
          lands 9 decrements later ($F7; $FD for X=0 where the counter
          only starts at the mode switch);
          q: T2 flag ($20) set at the first wrap, T2CL writes and IFR
          reads do not clear it; r: each T2CH write clears the flag,
          only X=0 wraps soon enough to re-set it before the read
          NOTE: m and n currently FAIL on m6522.h: the ACR write
          handler clears the T2 count pipeline on EVERY ACR write (not
          just on an actual PB6->clock mode transition), so one
          decrement is lost per loop iteration: -23 per read instead
          of -24 (the FIXME'd ACR handling in _m6522_write(),
          chips/m6522.h). o/p/q/r are unaffected because the T2CH
          reload resets the visible window each iteration
*/
static const subtest_t via5_subtests[VIA5_NUMTESTS] = {
    /* T1 counter registers written */
    { "a", "stx T1CL, read T1CL", -1, 0, -1, M6522_REG_T1CL, LOOP_STX_READ, M6522_REG_T1CL },
    { "b", "stx T1CL, read T1CH", -1, 0, -1, M6522_REG_T1CH, LOOP_STX_READ, M6522_REG_T1CL },
    { "c", "stx T1CH, read T1CL", -1, 0, -1, M6522_REG_T1CL, LOOP_STX_READ, M6522_REG_T1CH },
    { "d", "stx T1CH, read T1CH", -1, 0, -1, M6522_REG_T1CH, LOOP_STX_READ, M6522_REG_T1CH },
    { "e", "stx T1CL, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_READ, M6522_REG_T1CL },
    { "f", "stx T1CH, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_READ, M6522_REG_T1CH },
    /* T1 latch registers written */
    { "g", "stx T1LL, read T1LL", -1, 0, -1, M6522_REG_T1LL, LOOP_STX_READ, M6522_REG_T1LL },
    { "h", "stx T1LL, read T1LH", -1, 0, -1, M6522_REG_T1LH, LOOP_STX_READ, M6522_REG_T1LL },
    { "i", "stx T1LH, read T1LL", -1, 0, -1, M6522_REG_T1LL, LOOP_STX_READ, M6522_REG_T1LH },
    { "j", "stx T1LH, read T1LH", -1, 0, -1, M6522_REG_T1LH, LOOP_STX_READ, M6522_REG_T1LH },
    { "k", "stx T1LL, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_READ, M6522_REG_T1LL },
    { "l", "stx T1LH, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_READ, M6522_REG_T1LH },
    /* T2 registers written, ACR=0 (T2 counts clock) in the loop */
    { "m", "stx T2CL + ACR=0, read T2CL", -1, 0, -1, M6522_REG_T2CL, LOOP_STX_ACR0_READ, M6522_REG_T2CL },
    { "n", "stx T2CL + ACR=0, read T2CH", -1, 0, -1, M6522_REG_T2CH, LOOP_STX_ACR0_READ, M6522_REG_T2CL },
    { "o", "stx T2CH + ACR=0, read T2CL", -1, 0, -1, M6522_REG_T2CL, LOOP_STX_ACR0_READ, M6522_REG_T2CH },
    { "p", "stx T2CH + ACR=0, read T2CH", -1, 0, -1, M6522_REG_T2CH, LOOP_STX_ACR0_READ, M6522_REG_T2CH },
    { "q", "stx T2CL + ACR=0, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_ACR0_READ, M6522_REG_T2CL },
    { "r", "stx T2CH + ACR=0, read IFR",  -1, 0, -1, M6522_REG_IFR,  LOOP_STX_ACR0_READ, M6522_REG_T2CH },
};

/*--- via3a: IRQ flag reads after T1 latch writes -----------------------------*/

#define VIA3A_NUMTESTS 8

/*
    Like via3, but the setup write goes to the T1 LATCH registers
    (T1LL/T1LH = 1) instead of the counters; plain 14 cycle read loop
    reading IFR. A latch write never loads the counter and (on real
    hardware) never fires or affects flags, so the post-init counter
    oscillation persists in all sub tests: all reads are $20 (T2 flag
    only, ACR bit5=0 -> T2 counts clock and wraps).

    NOTE: b, d, f and h (T1 continuous mode) currently FAIL on
    m6522.h, same root cause as via3 b/d: m6522.h sets the T1 flag at
    every underflow while in continuous mode (reads $60 instead of
    $20); with latch=$0001 (b/d) or latch=$0100 (f/h) the oscillating
    counter underflows within a couple of ticks of the ACR write.
*/
static const subtest_t via3a_subtests[VIA3A_NUMTESTS] = {
    { "a", "T1LL=1, ACR=$00 (T1 one-shot), read IFR ($180d)",     M6522_REG_T1LL, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "b", "T1LL=1, ACR=$40 (T1 continuous), read IFR ($180d)",   M6522_REG_T1LL, 1, 0x40, M6522_REG_IFR, LOOP_READ, -1 },
    { "c", "T1LL=1, ACR=$80 (T1 one-shot, PB7), read IFR ($180d)", M6522_REG_T1LL, 1, 0x80, M6522_REG_IFR, LOOP_READ, -1 },
    { "d", "T1LL=1, ACR=$C0 (T1 contin., PB7), read IFR ($180d)", M6522_REG_T1LL, 1, 0xC0, M6522_REG_IFR, LOOP_READ, -1 },
    { "e", "T1LH=1, ACR=$00 (T1 one-shot), read IFR ($180d)",     M6522_REG_T1LH, 1, 0x00, M6522_REG_IFR, LOOP_READ, -1 },
    { "f", "T1LH=1, ACR=$40 (T1 continuous), read IFR ($180d)",   M6522_REG_T1LH, 1, 0x40, M6522_REG_IFR, LOOP_READ, -1 },
    { "g", "T1LH=1, ACR=$80 (T1 one-shot, PB7), read IFR ($180d)", M6522_REG_T1LH, 1, 0x80, M6522_REG_IFR, LOOP_READ, -1 },
    { "h", "T1LH=1, ACR=$C0 (T1 contin., PB7), read IFR ($180d)", M6522_REG_T1LH, 1, 0xC0, M6522_REG_IFR, LOOP_READ, -1 },
};

/*--- via9: T2 reads while toggling T2 clock/PB6 mode -------------------------*/

#define VIA9_NUMTESTS 12

/*
    T2 sibling of via4: setup [T2CL=1 | T2CH=1] + ACR=[$00 T2 counts
    clock | $20 T2 counts PB6], 24 cycle loop reading [T2CL|T2CH|IFR]
    and toggling ACR bit 5 every iteration ('lda $180b; eor #$20; sta
    $180b'). PB6 is static on the 1541, so T2 alternates between
    counting (clock phases) and stopped (PB6 phases): the counter
    advances ~12 per read on average (alternating -16/-8 windows in
    the reference). First read 12 ticks after the setup write.

      - a..c: T2CL=1 + ACR=$00: latch lo only, the counter (at $0000)
        wraps at the mode switch, first read at -5 ($FB); c: T2 flag
        set immediately, all reads $20
      - d..f: T2CH=1 + ACR=$00: counter loaded $0100, first read at -5,
        wraps (flag set) around read 21
      - g..i: T2CL=1 + ACR=$20: T2 stopped at $0000 until the first
        toggle to clock; i: flag set from read 1 on
      - j..l: T2CH=1 + ACR=$20: counter loaded $0100 and stopped until
        the first toggle; wraps around read 22; l: flag set then

    NOTE: a, b, d, e, f, g, h, j, k and l currently FAIL on m6522.h:
    every ACR write that leaves T2 in clock mode clears the T2 count
    pipeline (the FIXME'd ACR handler in _m6522_write(), chips/
    m6522.h), so one decrement is lost per toggle into clock mode and
    the counter falls behind the hardware more and more (counter
    reads and wrap/flag timing drift). c and i are unaffected because
    their flags fire right at the mode switch.
*/
static const subtest_t via9_subtests[VIA9_NUMTESTS] = {
    /* ACR=$00 group (starts clock counting, toggles $00<->$20) */
    { "a", "T2CL=1, ACR=$00, read T2CL", M6522_REG_T2CL, 1, 0x00, M6522_REG_T2CL, LOOP_READ_ACR_TOGGLE2, -1 },
    { "b", "T2CL=1, ACR=$00, read T2CH", M6522_REG_T2CL, 1, 0x00, M6522_REG_T2CH, LOOP_READ_ACR_TOGGLE2, -1 },
    { "c", "T2CL=1, ACR=$00, read IFR",  M6522_REG_T2CL, 1, 0x00, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE2, -1 },
    { "d", "T2CH=1, ACR=$00, read T2CL", M6522_REG_T2CH, 1, 0x00, M6522_REG_T2CL, LOOP_READ_ACR_TOGGLE2, -1 },
    { "e", "T2CH=1, ACR=$00, read T2CH", M6522_REG_T2CH, 1, 0x00, M6522_REG_T2CH, LOOP_READ_ACR_TOGGLE2, -1 },
    { "f", "T2CH=1, ACR=$00, read IFR",  M6522_REG_T2CH, 1, 0x00, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE2, -1 },
    /* ACR=$20 group (starts counting PB6 = stopped, toggles $20<->$00) */
    { "g", "T2CL=1, ACR=$20, read T2CL", M6522_REG_T2CL, 1, 0x20, M6522_REG_T2CL, LOOP_READ_ACR_TOGGLE2, -1 },
    { "h", "T2CL=1, ACR=$20, read T2CH", M6522_REG_T2CL, 1, 0x20, M6522_REG_T2CH, LOOP_READ_ACR_TOGGLE2, -1 },
    { "i", "T2CL=1, ACR=$20, read IFR",  M6522_REG_T2CL, 1, 0x20, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE2, -1 },
    { "j", "T2CH=1, ACR=$20, read T2CL", M6522_REG_T2CH, 1, 0x20, M6522_REG_T2CL, LOOP_READ_ACR_TOGGLE2, -1 },
    { "k", "T2CH=1, ACR=$20, read T2CH", M6522_REG_T2CH, 1, 0x20, M6522_REG_T2CH, LOOP_READ_ACR_TOGGLE2, -1 },
    { "l", "T2CH=1, ACR=$20, read IFR",  M6522_REG_T2CH, 1, 0x20, M6522_REG_IFR,  LOOP_READ_ACR_TOGGLE2, -1 },
};

/*--- via10: T1 PB7 output read back through port B ---------------------------*/

#define VIA10_NUMTESTS 8

/*
    Output timer A at PB7 and read it back through port B. Every sub
    test writes DDRB=$00 (all inputs), RB=$00, then [T1CL=1 | T1CH=1],
    then ACR, then reads RB ($1800) in the plain 14 cycle loop. RB
    reads back the pin levels: $1F (serial bus pull-ups on PB0..PB4,
    device jumpers ground PB5/PB6) with PB7 merged in when ACR bit 7
    (T1 output enable) is set.

    On real hardware the PB7 level is driven by the T1 output flip
    flop: writing T1CH sets PB7 high, the underflow clears it; in
    continuous mode it toggles at every underflow.

      - a,b: ACR=$00 (T1 one-shot, no PB7): plain $1F
      - c:   ACR=$80 (one-shot + PB7), T1CL=1: latch write only, the
        post-init counter state keeps PB7 high: $9F throughout
      - d:   ACR=$80, T1CH=1: PB7 high from the T1CH write, cleared at
        the underflow (+258 ticks): $9F for the first 18 reads, $1F
        from read 18 on
      - e,f: ACR=$40 (continuous, no PB7): plain $1F
      - g:   ACR=$C0 (continuous + PB7), T1CL=1: no underflows happen
        in the post-init counter state on real hardware, PB7 stays
        high: $9F throughout
      - h:   ACR=$C0, T1CH=1: PB7 square wave, toggling every ~258
        ticks: $9F for reads 0..17, then alternating ~18 read blocks
        of $1F/$9F

    NOTE (applies to via10..via14): the hardware T1 output flip flop
    is SET by an ACR bit7 0->1 transition, CLEARED by a T1CH write,
    SET at the underflow in one-shot mode and TOGGLED in continuous
    mode. m6522.h implements the last three (t1.t_bit) but NOT the
    ACR-set, so in via10/11/12/13 (ACR written after T1CH) d and h
    read back fully inverted ($1F where hardware shows $9F and vice
    versa) and g shows the post-init counter oscillation toggling PB7
    (real hardware does not underflow in this state at all, see the
    via3 b/d note). via14 writes the ACR first: there d and h pass
    (the post-init t_bit=true stands in for the missing ACR-set), g
    still fails on the post-init oscillation.
*/
typedef struct {
    const char* letter;
    const char* desc;
    uint8_t ddrb;        /* DDRB value written first */
    uint8_t prb;         /* RB value written second */
    int t1_reg;          /* T1CL or T1CH (written with 1) */
    uint8_t acr;
    bool acr_first;      /* via14: write ACR before the timer register */
} via10_subtest_t;

static const via10_subtest_t via10_subtests[VIA10_NUMTESTS] = {
    { "a", "DDRB=$00, ACR=$00 (one-shot, no PB7), T1CL=1", 0x00, 0x00, M6522_REG_T1CL, 0x00, false },
    { "b", "DDRB=$00, ACR=$00 (one-shot, no PB7), T1CH=1", 0x00, 0x00, M6522_REG_T1CH, 0x00, false },
    { "c", "DDRB=$00, ACR=$80 (one-shot, PB7),     T1CL=1", 0x00, 0x00, M6522_REG_T1CL, 0x80, false },
    { "d", "DDRB=$00, ACR=$80 (one-shot, PB7),     T1CH=1", 0x00, 0x00, M6522_REG_T1CH, 0x80, false },
    { "e", "DDRB=$00, ACR=$40 (contin., no PB7),   T1CL=1", 0x00, 0x00, M6522_REG_T1CL, 0x40, false },
    { "f", "DDRB=$00, ACR=$40 (contin., no PB7),   T1CH=1", 0x00, 0x00, M6522_REG_T1CH, 0x40, false },
    { "g", "DDRB=$00, ACR=$C0 (contin., PB7),      T1CL=1", 0x00, 0x00, M6522_REG_T1CL, 0xC0, false },
    { "h", "DDRB=$00, ACR=$C0 (contin., PB7),      T1CH=1", 0x00, 0x00, M6522_REG_T1CH, 0xC0, false },
};

/* via11: same, but with DDRB=$80 (PB7 additionally an output) */
#define VIA11_NUMTESTS 8

static const via10_subtest_t via11_subtests[VIA11_NUMTESTS] = {
    { "a", "DDRB=$80, ACR=$00 (one-shot, no PB7), T1CL=1", 0x80, 0x00, M6522_REG_T1CL, 0x00, false },
    { "b", "DDRB=$80, ACR=$00 (one-shot, no PB7), T1CH=1", 0x80, 0x00, M6522_REG_T1CH, 0x00, false },
    { "c", "DDRB=$80, ACR=$80 (one-shot, PB7),     T1CL=1", 0x80, 0x00, M6522_REG_T1CL, 0x80, false },
    { "d", "DDRB=$80, ACR=$80 (one-shot, PB7),     T1CH=1", 0x80, 0x00, M6522_REG_T1CH, 0x80, false },
    { "e", "DDRB=$80, ACR=$40 (contin., no PB7),   T1CL=1", 0x80, 0x00, M6522_REG_T1CL, 0x40, false },
    { "f", "DDRB=$80, ACR=$40 (contin., no PB7),   T1CH=1", 0x80, 0x00, M6522_REG_T1CH, 0x40, false },
    { "g", "DDRB=$80, ACR=$C0 (contin., PB7),      T1CL=1", 0x80, 0x00, M6522_REG_T1CL, 0xC0, false },
    { "h", "DDRB=$80, ACR=$C0 (contin., PB7),      T1CH=1", 0x80, 0x00, M6522_REG_T1CH, 0xC0, false },
};

/* via12: like via10, but with RB=$80 written (invisible with DDRB=$00) */
#define VIA12_NUMTESTS 8

static const via10_subtest_t via12_subtests[VIA12_NUMTESTS] = {
    { "a", "RB=$80, ACR=$00 (one-shot, no PB7), T1CL=1", 0x00, 0x80, M6522_REG_T1CL, 0x00, false },
    { "b", "RB=$80, ACR=$00 (one-shot, no PB7), T1CH=1", 0x00, 0x80, M6522_REG_T1CH, 0x00, false },
    { "c", "RB=$80, ACR=$80 (one-shot, PB7),     T1CL=1", 0x00, 0x80, M6522_REG_T1CL, 0x80, false },
    { "d", "RB=$80, ACR=$80 (one-shot, PB7),     T1CH=1", 0x00, 0x80, M6522_REG_T1CH, 0x80, false },
    { "e", "RB=$80, ACR=$40 (contin., no PB7),   T1CL=1", 0x00, 0x80, M6522_REG_T1CL, 0x40, false },
    { "f", "RB=$80, ACR=$40 (contin., no PB7),   T1CH=1", 0x00, 0x80, M6522_REG_T1CH, 0x40, false },
    { "g", "RB=$80, ACR=$C0 (contin., PB7),      T1CL=1", 0x00, 0x80, M6522_REG_T1CL, 0xC0, false },
    { "h", "RB=$80, ACR=$C0 (contin., PB7),      T1CH=1", 0x00, 0x80, M6522_REG_T1CH, 0xC0, false },
};

/* via13: DDRB=$80 and RB=$80: PB7 driven high by the port register
   whenever the timer output does not override it */
#define VIA13_NUMTESTS 8

static const via10_subtest_t via13_subtests[VIA13_NUMTESTS] = {
    { "a", "DDRB/RB=$80, ACR=$00 (one-shot, no PB7), T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x00, false },
    { "b", "DDRB/RB=$80, ACR=$00 (one-shot, no PB7), T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x00, false },
    { "c", "DDRB/RB=$80, ACR=$80 (one-shot, PB7),     T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x80, false },
    { "d", "DDRB/RB=$80, ACR=$80 (one-shot, PB7),     T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x80, false },
    { "e", "DDRB/RB=$80, ACR=$40 (contin., no PB7),   T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x40, false },
    { "f", "DDRB/RB=$80, ACR=$40 (contin., no PB7),   T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x40, false },
    { "g", "DDRB/RB=$80, ACR=$C0 (contin., PB7),      T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0xC0, false },
    { "h", "DDRB/RB=$80, ACR=$C0 (contin., PB7),      T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0xC0, false },
};

/* via14: like via13, but the ACR is written *before* the timer
   register (the order real code would use) */
#define VIA14_NUMTESTS 8

static const via10_subtest_t via14_subtests[VIA14_NUMTESTS] = {
    { "a", "ACR first: ACR=$00 (one-shot, no PB7), T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x00, true },
    { "b", "ACR first: ACR=$00 (one-shot, no PB7), T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x00, true },
    { "c", "ACR first: ACR=$80 (one-shot, PB7),     T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x80, true },
    { "d", "ACR first: ACR=$80 (one-shot, PB7),     T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x80, true },
    { "e", "ACR first: ACR=$40 (contin., no PB7),   T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0x40, true },
    { "f", "ACR first: ACR=$40 (contin., no PB7),   T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0x40, true },
    { "g", "ACR first: ACR=$C0 (contin., PB7),      T1CL=1", 0x80, 0x80, M6522_REG_T1CL, 0xC0, true },
    { "h", "ACR first: ACR=$C0 (contin., PB7),      T1CH=1", 0x80, 0x80, M6522_REG_T1CH, 0xC0, true },
};

/*--- via20/via21: IFR reads with in-loop flag acks ----------------------------*/

/*
    PCR=$EE (CA2/CB2 fixed output high, no handshake IRQs), ACR=$50
    (T1 continuous, T2 shift-register free running mode), then T1 and
    T2 programmed with small periods. T2 is written through latch lo +
    counter hi (always loads), T1 via the latch registers (via20) or
    the counter registers (via21, force-load). The 18 cycle loop reads
    IFR, writes the value back (acking the observed flags), stores it
    and repeats; the ~18 cycle loop vs the T1 period (~T1+2) slowly
    shifts the sampling phase against the timer IRQs.

    On real hardware this answers: does T2 in SR free running mode
    generate IRQs (yes - it reloads from the latch lo byte and fires
    periodically), and does a $FFFF underflow generate an IRQ in 8-bit
    mode.

    NOTE: all sub tests currently FAIL on m6522.h: the shift register
    and the T2 8-bit free-running mode are not implemented (SR FIXMEs
    in chips/m6522.h) - its T2 is a 16-bit one-shot that never reloads,
    so the periodic T2 IRQs ($20 blips) of the hardware are missing
    after the first underflow. via20 additionally suffers from the
    post-init counter oscillation issues (T1 not force-loaded, see the
    via3 b/d note), via21's T1 is force-loaded and behaves.
*/
typedef struct {
    const char* letter;
    const char* desc;
    uint16_t t1;
    uint16_t t2;
    uint8_t acr;
    bool t1_counter;    /* via21: program T1 through T1CL/T1CH */
} via20_subtest_t;

#define VIA20_NUMTESTS 12
#define VIA21_NUMTESTS 12

#define VIA20_TEST(letter, t1, t2) { letter, "T1=$" #t1 ", T2=$" #t2 ", ACR=$50", 0x##t1, 0x##t2, 0x50, false }
#define VIA21_TEST(letter, t1, t2) { letter, "T1=$" #t1 ", T2=$" #t2 ", ACR=$50", 0x##t1, 0x##t2, 0x50, true }

static const via20_subtest_t via20_subtests[VIA20_NUMTESTS] = {
    VIA20_TEST("a", 0013, 0114),
    VIA20_TEST("b", 0014, 0115),
    VIA20_TEST("c", 0015, 0116),
    VIA20_TEST("d", 0017, 0118),
    VIA20_TEST("e", 0018, 0119),
    VIA20_TEST("f", 0028, 0101),
    VIA20_TEST("g", 0038, 0102),
    VIA20_TEST("h", 0048, 0103),
    VIA20_TEST("i", 0050, 0104),
    VIA20_TEST("j", 0051, 0104),
    VIA20_TEST("k", 0052, 0104),
    VIA20_TEST("l", 0053, 0117),
};

static const via20_subtest_t via21_subtests[VIA21_NUMTESTS] = {
    VIA21_TEST("a", 0013, 0114),
    VIA21_TEST("b", 0014, 0115),
    VIA21_TEST("c", 0015, 0116),
    VIA21_TEST("d", 0017, 0118),
    VIA21_TEST("e", 0018, 0119),
    VIA21_TEST("f", 0028, 0101),
    VIA21_TEST("g", 0038, 0102),
    VIA21_TEST("h", 0048, 0103),
    VIA21_TEST("i", 0050, 0104),
    VIA21_TEST("j", 0051, 0104),
    VIA21_TEST("k", 0052, 0104),
    VIA21_TEST("l", 0053, 0117),
};

static void run_via20(const via20_subtest_t* t, uint8_t* buf) {
    m6522_init(&via);
    drivecode_prologue();
    idle(CY_LDA_IMM);                       /* lda #$EE */
    i_sta_abs(M6522_REG_PCR, 0xEE);         /* sta PCR (CA2/CB2 fixed high) */
    idle(CY_LDA_IMM);                       /* lda #.ACR */
    i_sta_abs(M6522_REG_ACR, t->acr);       /* sta ACR */
    idle(CY_LDA_IMM);                       /* lda #<T1 */
    i_sta_abs(t->t1_counter ? M6522_REG_T1CL : M6522_REG_T1LL, t->t1 & 0xFF);
    idle(CY_LDA_IMM);                       /* lda #<T2 */
    i_sta_abs(M6522_REG_T2CL, t->t2 & 0xFF);
    idle(CY_LDA_IMM);                       /* lda #>T1 */
    i_sta_abs(t->t1_counter ? M6522_REG_T1CH : M6522_REG_T1LH, t->t1 >> 8);
    idle(CY_LDA_IMM);                       /* lda #>T2 */
    i_sta_abs(M6522_REG_T2CH, t->t2 >> 8);  /* sta T2CH (loads counter) */
    idle(CY_LDX_IMM);                       /* ldx #0 */
    for (int i = 0; i < 256; i++) {
        uint8_t ifr = i_lda_abs(M6522_REG_IFR);     /* lda IFR */
        idle(CY_STA_ABS - 1);                        /* sta IFR (write back) */
        wr_cycle(M6522_REG_IFR, ifr);                /*   = ack observed flags */
        buf[i] = ifr;
        if (i < 255) {
            idle(CY_STA_ABSX + CY_INX + CY_BNE_TAKEN);   /* 18 cycle loop */
        }
    }
}

static void run_via10(const via10_subtest_t* t, uint8_t* buf) {
    m6522_init(&via);
    drivecode_prologue();
    idle(CY_LDA_IMM);                       /* lda #.DDRB */
    i_sta_abs(M6522_REG_DDRB, t->ddrb);     /* sta DDRB */
    idle(CY_LDA_IMM);                       /* lda #.PRB */
    i_sta_abs(M6522_REG_RB, t->prb);        /* sta RB */
    if (t->acr_first) {
        idle(CY_LDA_IMM);                   /* lda #.CR */
        i_sta_abs(M6522_REG_ACR, t->acr);   /* sta ACR */
        idle(CY_LDA_IMM);                   /* lda #1 */
        i_sta_abs((uint8_t)t->t1_reg, 1);   /* sta T1CL/T1CH */
    }
    else {
        idle(CY_LDA_IMM);                   /* lda #1 */
        i_sta_abs((uint8_t)t->t1_reg, 1);   /* sta T1CL/T1CH */
        idle(CY_LDA_IMM);                   /* lda #.CR */
        i_sta_abs(M6522_REG_ACR, t->acr);   /* sta ACR */
    }
    idle(CY_LDX_IMM);                       /* ldx #0 */
    for (int i = 0; i < 256; i++) {
        buf[i] = i_lda_abs(M6522_REG_RB);   /* lda RB */
        if (i < 255) {
            idle(CY_STA_ABSX + CY_INX + CY_BNE_TAKEN);
        }
    }
}

/*--- test group registry ---------------------------------------------------*/

typedef struct {
    const char* name;               /* original test program name */
    int num;                        /* number of sub tests */
    const subtest_t* subtests;
    const uint8_t* ref;             /* num * 256 reference bytes */
} testgroup_t;

static const testgroup_t groups[] = {
    { "via1", VIA1_NUMTESTS, via1_subtests, via1_ref },
    { "via2", VIA2_NUMTESTS, via2_subtests, via2_ref },
    { "via3", VIA3_NUMTESTS, via3_subtests, via3_ref },
    { "via3a", VIA3A_NUMTESTS, via3a_subtests, via3a_ref },
    { "via4", VIA4_NUMTESTS, via4_subtests, via4_ref },
    { "via5", VIA5_NUMTESTS, via5_subtests, via5_ref },
    { "via9", VIA9_NUMTESTS, via9_subtests, via9_ref },
};

_Static_assert(VIA1_NUMTESTS*256 == VIA1_REF_SIZE, "via1 ref size mismatch");
_Static_assert(VIA2_NUMTESTS*256 == VIA2_REF_SIZE, "via2 ref size mismatch");
_Static_assert(VIA3_NUMTESTS*256 == VIA3_REF_SIZE, "via3 ref size mismatch");
_Static_assert(VIA4_NUMTESTS*256 == VIA4_REF_SIZE, "via4 ref size mismatch");
_Static_assert(VIA3A_NUMTESTS*256 == VIA3A_REF_SIZE, "via3a ref size mismatch");
_Static_assert(VIA5_NUMTESTS*256 == VIA5_REF_SIZE, "via5 ref size mismatch");
_Static_assert(VIA9_NUMTESTS*256 == VIA9_REF_SIZE, "via9 ref size mismatch");

int main(void) {
    static uint8_t buf[256];

    printf("m6522 viavarious tests (ported from vice-testprogs/drive/viavarious/)\n");
    int failed = 0;
    for (size_t g = 0; g < sizeof(groups)/sizeof(groups[0]); g++) {
        const testgroup_t* grp = &groups[g];
        printf("%s:\n", grp->name);
        int grp_failed = 0;
        for (int i = 0; i < grp->num; i++) {
            run_subtest(&grp->subtests[i], buf);
            if (!report(grp->subtests[i].letter, grp->subtests[i].desc, buf, &grp->ref[i*256])) {
                grp_failed++;
            }
        }
        printf("%s: %d/%d sub tests passed, %d failed\n",
               grp->name, grp->num - grp_failed, grp->num, grp_failed);
        failed += grp_failed;
    }

    {
        const struct { const char* name; int num; const via10_subtest_t* subtests; const uint8_t* ref; } pb7_groups[] = {
            { "via10", VIA10_NUMTESTS, via10_subtests, via10_ref },
            { "via11", VIA11_NUMTESTS, via11_subtests, via11_ref },
            { "via12", VIA12_NUMTESTS, via12_subtests, via12_ref },
            { "via13", VIA13_NUMTESTS, via13_subtests, via13_ref },
            { "via14", VIA14_NUMTESTS, via14_subtests, via14_ref },
        };
        const struct { const char* name; int num; const via20_subtest_t* subtests; const uint8_t* ref; } via20_groups[] = {
            { "via20", VIA20_NUMTESTS, via20_subtests, via20_ref },
            { "via21", VIA21_NUMTESTS, via21_subtests, via21_ref },
        };
        for (size_t g = 0; g < sizeof(pb7_groups)/sizeof(pb7_groups[0]); g++) {
            printf("%s:\n", pb7_groups[g].name);
            int grp_failed = 0;
            for (int i = 0; i < pb7_groups[g].num; i++) {
                run_via10(&pb7_groups[g].subtests[i], buf);
                if (!report(pb7_groups[g].subtests[i].letter, pb7_groups[g].subtests[i].desc,
                            buf, &pb7_groups[g].ref[i*256])) {
                    grp_failed++;
                }
            }
            printf("%s: %d/%d sub tests passed, %d failed\n",
                   pb7_groups[g].name, pb7_groups[g].num - grp_failed, pb7_groups[g].num, grp_failed);
            failed += grp_failed;
        }
        for (size_t g = 0; g < sizeof(via20_groups)/sizeof(via20_groups[0]); g++) {
            printf("%s:\n", via20_groups[g].name);
            int grp_failed = 0;
            for (int i = 0; i < via20_groups[g].num; i++) {
                run_via20(&via20_groups[g].subtests[i], buf);
                if (!report(via20_groups[g].subtests[i].letter, via20_groups[g].subtests[i].desc,
                            buf, &via20_groups[g].ref[i*256])) {
                    grp_failed++;
                }
            }
            printf("%s: %d/%d sub tests passed, %d failed\n",
                   via20_groups[g].name, via20_groups[g].num - grp_failed, via20_groups[g].num, grp_failed);
            failed += grp_failed;
        }
    }

    printf("viavarious: %d sub tests failed\n", failed);
    return failed;
}
