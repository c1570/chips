; ---------------------------------------------------------------------------
; quick and dirty LLM converted ASM version (ACME syntax) of
; Jochen "NLQ" Adler's iecdelay test program available at
; https://sourceforge.net/p/vice-emu/code/HEAD/tree/testprogs/drive/iecdelay/
; ---------------------------------------------------------------------------

!convtab pet

; --- KERNAL ----------------------------------------------------------
setlfs  = $ffba
setnam  = $ffbd
open    = $ffc0
close   = $ffc3
chkout  = $ffc9
chkin   = $ffc6
getin_k = $ffe4
clrchn  = $ffcc
chrout  = $ffd2
getin   = $ffe4

; --- hardware / system -----------------------------------------------
dd00    = $dd00         ; CIA2 IEC port
d011    = $d011         ; VIC control (bit 4 = display enable)
d020    = $d020         ; border colour
curcol  = $d3           ; screen editor: cursor column
sysa    = $030c         ; SYS A register (poked by original, unused)
status  = $90           ; KERNAL I/O status
colram  = $d7ff         ; 55295 (line 5000/5001)

; --- PETSCII ---------------------------------------------------------
clrscr  = $93
down    = $11
up      = $91
crsr_l  = $9d           ; {left}
crsr_r  = $1d           ; {rght}
dish    = $08           ; {dish}  disable shift+CBM
swlc    = $0e           ; {swlc}  lowercase charset

; --- zero page -------------------------------------------------------
; BASIC's free cells $FB/$FC are used by the measurement routine itself,
; so we borrow them only OUTSIDE jsr meas (it re-inits them).
ptr1    = $fb           ; pointer for prstr/sendc (lo=$FB, hi=$FC)
len1    = $fd           ; length for sendc ($FC would collide with ptr hi!)

; --- constants from BASIC (line 1000-1525) ---------------------------
alval   = 10000         ; al = amount of loops
bval    = 65536-alval   ; loop counter preload: 216*256+240
fqcyc   = 27587         ; r*27587: 7*fq/al scaled by 400000 (see calc_rd)
qnum    = 4000          ; us_th = (rd_th*4000+1970)/3941  (1/fq scaling)
qden    = 3941

; =========================================================================
!to "iecdelay-asm.prg",cbm
* = $0801
!byte $0c,$08,$0a,$00,$9e,$20,$32,$30,$36,$34,$00,$00,$00   ; 10 SYS 2064

!macro prs str {
        lda #<str
        ldy #>str
        jsr prstr
}

* = $0810
; =========================================================================
; main driver (the surrounding BASIC program)
; =========================================================================
main:
        lda #'-'
        sta dbg1
        ; 1010 print"{clr}{dish}{swlc}"
        lda #clrscr
        jsr chrout
        lda #swlc
        jsr chrout
        lda #dish
        jsr chrout

        ; 1540 open 5,8,15
        ldx #0
        stx rttry
@retry: ldx #5
        jsr close           ; a failed OPEN leaves the file registered!
        lda #5
        ldx #8
        ldy #15
        jsr setlfs
        lda #0
        ldx #0
        ldy #0
        jsr setnam
        jsr open
        bcc +
        ; drive may still be settling after LOAD: wait and retry
        lda #0
        sta rttry            ; attempt counter
@wait:  ldx #0
        ldy #0
@w2:    inx
        bne @w2
        iny
        bne @w2              ; ~0.26s delay
        inc rttry
        lda rttry
        cmp #8               ; up to ~2s total
        bcc @retry
        lda #1
        sta dbg1
        jmp deverr
+
        ; 1550 print#5,"U0>M0"   (1571 -> 1541 mode)
        lda #<s_u0m0
        ldy #>s_u0m0
        ldx #5
        jsr sendc
        bcc +
        jmp deverr
+

        ; 1640-1660: upload drive code (M-W $0500, 39 bytes)
        jsr upload
        bcc +
        jmp deverr
+

        ; -----------------------------------------------------------
        ; 1670 for fl=0 to 1 : 1680 for c6=0 to 1
        ; -----------------------------------------------------------
        lda #0
        sta fl_idx
fl_loop:
        lda #0
        sta c6_idx
c6_loop:
        ; 1690/1700 print edges
        +prs s_flop
        lda fl_idx
        jsr pr_fr
        +prs s_edge
        +prs s_c64l
        lda c6_idx
        jsr pr_fr
        +prs s_edge

        ; 1710-1740: c64 branch <- floppy edge (beq=$f0 / bne=$d0)
        lda fl_idx
        beq @flfall
        lda #$d0
        ldx #$02            ; 1750/1760: ora #$02 (rising)
        jmp @flset
@flfall:
        lda #$f0
        ldx #$00            ; ora #$00 (falling)
@flset: sta m_branch       ; 1740 poke 49183
        sta d_ora+1         ; 1770 poke 49408+7 (byte 7 of the drive code)
        ; upload ora operand to drive $0507
        stx mwbuf+6
        lda #(d_ora+1-drv)
        sta mwbuf+3
        lda #$05
        sta mwbuf+4
        lda #$01
        sta mwbuf+5
        ldx #7
        jsr sendc
        bcc +
        jmp deverr
+

        ; 1780-1800: drive branch <- c64 edge
        lda c6_idx
        beq @c6fall
        lda #$d0
        jmp @c6set
@c6fall:
        lda #$f0
@c6set: sta d_branch
        sta mwbuf+6
        lda #(d_branch-drv)
        sta mwbuf+3
        lda #$05
        sta mwbuf+4
        lda #$01
        sta mwbuf+5
        ldx #7
        jsr sendc
        bcc +
        jmp deverr
+

        ; 1810 print#5,"m-e"chr$(0)chr$(5)
        lda #<s_me
        ldy #>s_me
        ldx #5
        jsr sendc
        bcc +
        jmp deverr
+

        ; 1820 if c6=1 then poke 56576,23 (c64 clock-out active)
        lda c6_idx
        beq @noclk
        lda #23
        sta dd00
@noclk:
        ; 1710 for i=49166 to 49177: poke i,234
        ; fresh NOPs across the whole delay area every combo (a stale
        ; byte beyond a shorter build executes as garbage/BRK otherwise)
        ldx #0
@nf:    lda #$ea
        sta m_delay,x
        inx
        cpx #12
        bcc @nf

        ; -----------------------------------------------------------
        ; 1830 for dl=2 to 11  (delay in cycles)
        ; -----------------------------------------------------------
        lda #2
        sta dl_cur
dl_loop:
        ; 1840 poke 53265,peek(53265) and 239   (display off)
        lda d011
        and #$ef
        sta d011

        ; 1850-2030: build delay code in the meas delay area
        jsr build_delay

        ; 2040-2080: loop counters, 65536-al
        lda #<bval
        sta $fb
        lda #>bval
        sta $fc

        ; 2090 bc=peek(53280)
        lda d020
        sta bc_save

        ; 2100/2110 poke 780,215/199 (SYS A reg; unused by routine,
        ; kept for fidelity with the original)
        lda c6_idx
        beq @sysfall
        lda #199
        jmp @sysset
@sysfall:
        lda #215
@sysset:
        sta sysa

        ; 2120 sys 49152    (original; runs the relocated meas routine)
        jsr meas

        ; 2130/2140 restore border, display on
        lda bc_save
        sta d020
        lda d011
        ora #$10
        sta d011

        ; 2150 r=peek(253)+peek(254)*256 -> store in r(fl,c6,dl)
        lda dl_cur
        sta dli
        jsr r_index         ; Y = byte offset into r_tab
        lda $fd
        sta r_tab,y
        lda $fe
        sta r_tab+1,y

        ; 2190 print" delay cycles:";dl;"pos.responses:";int(...)"{left}%"
        +prs s_dcyc
        lda dl_cur
        ldy #0
        jsr prnum16
        +prs s_presp
        clc                 ; r1 = (r+50)/100
        lda $fd
        adc #50
        sta num16
        lda $fe
        adc #0
        sta num16+1
        lda #100
        jsr div16by         ; q16 = r1
        lda q16
        ldy q16+1
        jsr prnum16
        +prs s_pct

        ; next dl
        inc dl_cur
        lda dl_cur
        cmp #12
        bcc @next
        jmp dl_end
@next:  jmp dl_loop

dl_end:
        ; 2220 poke 53265,peek(53265) or 16
        lda d011
        ora #$10
        sta d011

        ; 2230/2240 print edges again
        +prs s_flop
        lda fl_idx
        jsr pr_fr
        +prs s_edge
        +prs s_c64l
        lda c6_idx
        jsr pr_fr
        +prs s_edge

        ; 2250-2280 stats header
        +prs s_relhdr1
        +prs s_relhdr2
        +prs s_relhdr3
        +prs s_relhdr4

        ; 2290 c=0 (counter of good results)
        lda #0
        sta c_cnt

        ; 2300 for i=2 to 11
        lda #2
        sta i_cur
st_loop:
        ; r = r(fl,c6,i)
        jsr r_fetch

        ; r1 = int(r/al*100+0.5) = (r+50)/100
        clc
        lda r_cur
        adc #50
        sta num16
        lda r_cur+1
        adc #0
        sta num16+1
        lda #100
        jsr div16by

        ; 2330 print"  "i,r1"{left}%",
        +prs s_row2
        lda i_cur
        ldy #0
        jsr prnum16
        lda #10
        jsr tabcol
        lda q16
        ldy q16+1
        jsr prnum16
        lda #crsr_l
        jsr chrout
        lda #'%'
        jsr chrout
        lda #20
        jsr tabcol

        ; 2340 if r1=0 or r1=100 then skip row
        lda q16
        beq @skiprow
        cmp #100
        beq @skiprow

        ; 2350 rd(i) = i - 100*r/al/dv  (thousandths)
        jsr calc_rd
        jsr rd_store

        ; 2360 print int(rd*100+0.5)/100 ,
        lda h1_cur
        sta num16
        lda h1_cur+1
        sta num16+1
        jsr pr_h

        lda #30
        jsr tabcol

        ; 2370 print int(rd/fq*100+0.5)/100 ;
        lda h2_cur
        sta num16
        lda h2_cur+1
        sta num16+1
        jsr pr_h

        ; 2380/2390 a(c)=rd(i): c=c+1
        jsr a_store
@skiprow:
        ; 2400 print (newline)
        lda #$0d
        jsr chrout

        ; next i
        inc i_cur
        lda i_cur
        cmp #12
        bcc @nxt
        jmp st_end
@nxt:   jmp st_loop
st_end:

        ; 2420 if c=0 then skip average
        lda c_cnt
        beq avg_skip

        ; 2430-2480 rd = sum/c : ra(fl,c6)=rd
        jsr calc_avg
        lda fl_idx
        asl
        clc
        adc c6_idx
        asl
        tay
        lda ra_th
        sta ra_tab,y
        lda ra_th+1
        sta ra_tab+1,y

        ; 2490-2510 print averages
        +prs s_avgh
        +prs s_sp
        lda ra_th
        sta num16
        lda ra_th+1
        sta num16+1
        jsr pr_fp
        +prs s_c64cyc
        +prs s_sp
        lda ra_th
        sta num16
        lda ra_th+1
        sta num16+1
        jsr calc_us
        lda us_cur
        sta num16
        lda us_cur+1
        sta num16+1
        jsr pr_fp
        +prs s_usec
avg_skip:
        ; 2520 poke 56576,31 : 15 : 7
        lda #31
        sta dd00
        lda #15
        sta dd00
        lda #7
        sta dd00

        ; TEST BUILD: end after the first run (falling/falling)
        rts
fl_end:
        ; 2560 print#5,"ui"
        lda #<s_ui
        ldy #>s_ui
        ldx #2
        jsr sendc

        ; -----------------------------------------------------------
        ; 2570-2980 results pages, then 3120 goto 2570 (loop)
        ; -----------------------------------------------------------
pages:
        +prs s_p1h
        +prs s_p1a
        lda #20
        jsr tabcol
        lda #<s_fall
        ldy #>s_fall
        jsr prstr
        lda #30
        jsr tabcol
        lda #<s_rise
        ldy #>s_rise
        jsr prstr
        lda #$0d
        jsr chrout

        ; 2590 " C64 falling edge" , ra(0,0) , ra(1,0)
        +prs s_p1r1
        lda #20
        jsr tabcol
        lda #0
        jsr pr_cyc
        lda #30
        jsr tabcol
        lda #4
        jsr pr_cyc
        lda #$0d
        jsr chrout

        ; 2600 " C64 rising edge" , ra(0,1) , ra(1,1)
        +prs s_p1r2
        lda #20
        jsr tabcol
        lda #2
        jsr pr_cyc
        lda #30
        jsr tabcol
        lda #6
        jsr pr_cyc
        lda #$0d
        jsr chrout

        +prs s_p1b
        +prs s_p1c
        lda #20
        jsr tabcol
        lda #<s_fall
        ldy #>s_fall
        jsr prstr
        lda #30
        jsr tabcol
        lda #<s_rise
        ldy #>s_rise
        jsr prstr
        lda #$0d
        jsr chrout

        ; 2640/2650 falling row in microseconds
        +prs s_p1r1
        lda #20
        jsr tabcol
        lda #0
        jsr pr_us
        lda #30
        jsr tabcol
        lda #4
        jsr pr_us
        lda #$0d
        jsr chrout

        ; 2660/2670 rising row in microseconds
        +prs s_p1r2
        lda #20
        jsr tabcol
        lda #2
        jsr pr_us
        lda #30
        jsr tabcol
        lda #6
        jsr pr_us
        lda #$0d
        jsr chrout

        +prs s_p1d
        +prs s_p1e
        +prs s_p1f
        +prs s_p1g
        +prs s_p1h2
        +prs s_p1r
        jsr pause           ; 2731 input a$

        ; 2740 results (2/2)
        +prs s_p2h
        +prs s_p2a
        +prs s_p2b
        +prs s_p2c
        +prs s_p2d
        +prs s_p2e
        +prs s_p2f
        +prs s_p2g

        ; 2840-2870 x row
        jsr calc_x4
        +prs s_xr
        lda #0
        sta xloop
@xr:    lda xloop
        asl
        tay
        lda x_tab,y
        sta num16
        lda x_tab+1,y
        sta num16+1
        jsr pr_fp
        jsr pr_lus
        lda xloop
        bne @x1
        lda #10
        jsr tabcol
        jmp @xnext
@x1:    lda xloop
        cmp #1
        bne @x2
        lda #20
        jsr tabcol
        jmp @xnext
@x2:    lda xloop
        cmp #2
        bne @x3
        lda #30
        jsr tabcol
        lda #' '
        jsr chrout
        jmp @xnext
@x3:    lda #' '
        jsr chrout
        lda #' '
        jsr chrout
        lda #' '
        jsr chrout
        lda #' '
        jsr chrout
        lda #' '
        jsr chrout
@xnext: inc xloop
        lda xloop
        cmp #4
        bcc @xn2
        jmp @xrdone
@xn2:   jmp @xr
@xrdone:

        ; 2870 print"     "; : poke 53280,5
        ldx #0
@sp5:   lda #' '
        jsr chrout
        inx
        cpx #5
        bcc @sp5
        lda #5
        sta d020

        ; 2871-2874 gosub 4000 for each column
        ldx #0
@sl:    txa
        asl
        tay
        lda x_tab,y
        sta num16
        lda x_tab+1,y
        sta num16+1
        sec                 ; num16 < mi ?
        lda num16
        sbc mi_tab,x
        lda num16+1
        sbc mi_tab+4,x
        bcc @lt
        sec                 ; num16 > ma ? (ma < num16)
        lda ma_tab,x
        sbc num16
        lda ma_tab+4,x
        sbc num16+1
        bcc @gt
        +prs s_ok
        jmp @snext
@lt:    +prs s_min
        jmp @sred
@gt:    +prs s_max
@sred:  lda #10
        sta d020
@snext: inx
        cpx #4
        bcc @sl

        ; 2880-2960 legends
        lda #$0d
        jsr chrout
        +prs s_leg1
        +prs s_leg2
        +prs s_leg3
        +prs s_leg4
        +prs s_leg5
        +prs s_leg6
        +prs s_p2r
        lda #0              ; 2975 gosub 5000 (net effect of the
        sta colram          ; original's "and 15 = 10" precedence quirk)
        jsr pause           ; 2980 input a$

        jmp pages           ; 3120 goto 2570

deverr:
        lda status
        sta dbg2
        lda $98
        sta dbg3
        lda $0259
        sta dbg4
        ldy #0
        lda dbg1
        jsr dbgn
        lda dbg2
        jsr dbgn
        lda dbg3
        jsr dbgn
        lda dbg4
        jsr dbgn
        +prs s_devnp
        rts                 ; back to BASIC like ?DEVICE NOT PRESENT

; dbgn: write A as two hex screen codes at $04C8,Y (row 3), advance Y
dbgn:   pha
        lsr
        lsr
        lsr
        lsr
        jsr @n
        pla
        and #$0f
@n:     cmp #$0a
        bcc @d
        adc #$06
@d:     ora #$30
        sta $04c8,y
        iny
        rts

; =========================================================================
; subroutines
; =========================================================================

; pr_fr: print fr$(a) - "falling"/"rising"
pr_fr:  cmp #0
        beq @fall
        lda #<s_rise
        ldy #>s_rise
        jsr prstr
        rts
@fall:  lda #<s_fall
        ldy #>s_fall
        jsr prstr
        rts

; sendc: A/Y = ptr, X = length. PRINT#5 via CHKOUT/BSOUT + CR.
; returns carry set on device-not-present.
sendc:  sta ptr1
        sty ptr1+1
        stx len1
        lda #3              ; retry attempts
        sta rttry
@again: ldx #5              ; CHKOUT takes the LA in X ($F30F does TXA)!
        jsr chkout
        bcc @ok
        jmp @np
@ok:    ldy #0
@l:     lda (ptr1),y
        jsr chrout
        iny
        cpy len1
        bne @l
        lda #$0d
        jsr chrout
        jsr clrchn
        lda status
        bpl @done           ; no device-not-present bit
        ; drive busy/not present: wait a beat and retry
        dec rttry
        beq @np
        ldx #0
        ldy #0
@rd:    inx
        bne @rd
        iny
        bne @rd
        jmp @again
@done:  clc
        rts
@np:    jsr clrchn          ; restore screen as output device!
        sec
        rts

; upload: 39 bytes to drive $0500.
; M-W format from the 1541 manual:
;   "M-W" <addr lo> <addr hi> <count> <data...>
; At most 34 data bytes per command, so use two M-Ws: 34 bytes at $0500
; and the remaining 5 at $0522 (not 39 one-byte commands).
upload: lda #0
        sta dbg1            ; final status

        ; --- M-W $0500, 34 bytes ---------------------------------
        ldx #5
        jsr chkout
        bcc @u1
        jmp @err
@u1:    lda #$4d            ; 'M'
        jsr chrout
        lda #$2d            ; '-'
        jsr chrout
        lda #$57            ; 'W'
        jsr chrout
        lda #$00
        jsr chrout          ; addr lo
        lda #$05
        jsr chrout          ; addr hi
        lda #34
        jsr chrout          ; count
        ldx #0
@l1:    lda drv,x
        jsr chrout
        inx
        cpx #34
        bcc @l1
        lda #$0d
        jsr chrout          ; CR ends this M-W
        lda status
        sta dbg1
        jsr clrchn
        lda dbg1
        and #$1f
        beq @u2
        jmp @err
@u2:
        ; --- M-W $0522, 5 bytes ----------------------------------
        ldx #5
        jsr chkout
        bcc @u3
        jmp @err
@u3:    lda #$4d            ; 'M'
        jsr chrout
        lda #$2d            ; '-'
        jsr chrout
        lda #$57            ; 'W'
        jsr chrout
        lda #$22
        jsr chrout          ; addr lo ($0500+34)
        lda #$05
        jsr chrout          ; addr hi
        lda #5
        jsr chrout          ; count
        ldx #34
@l2:    lda drv,x
        jsr chrout
        inx
        cpx #39
        bcc @l2
        lda #$0d
        jsr chrout          ; CR ends this M-W
        lda status
        sta dbg1
        sta dbg4
        jsr clrchn
        lda dbg1
        and #$1f
        beq @u4
        jmp @err
@u4:    clc
        rts
@err:   jsr clrchn
        sec                  ; flag errors
        rts

; verify: M-R the 39 bytes back and compare with drv[]
verify: lda #'M'
        sta mwbuf
        lda #'R'
        sta mwbuf+1
        lda #0
        sta mwbuf+3
        lda #$05
        sta mwbuf+4
        lda #$27
        sta mwbuf+5
        ldx #7
        jsr sendc
        ldy #0
        ldx #0
@rdly:  inx
        bne @rdly
        iny
        bne @rdly            ; let the drive process the M-R
        ldx #5
        jsr chkin
        lda #0
        sta upidx
@vlp:   jsr getin
        ldy upidx
        cmp drv,y
        bne @vmis
        inc upidx
        lda upidx
        cmp #39
        bcc @vlp
        jsr clrchn
        clc
        rts
@vmis:  sta dbg2            ; got
        lda drv,y
        sta dbg3            ; expected
        lda upidx
        clc
        adc #$40
        sta dbg1            ; $40+index = verify mismatch marker
        jsr clrchn
        sec
        rts

; build_delay: 1850-2030, patches the delay area in the meas routine..
;   even dl: dl/2 nops, lda $dd00, bit $ff
;   odd  dl: bit $ff, (dl-3)/2 nops, lda $dd00, nop nop
build_delay:
        lda dl_cur
        sta td_var
        ldy #0              ; Y = offset within the 12-byte delay area
        lda dl_cur
        lsr
        bcc @nops           ; even
        lda #$24            ; bit $ff (3 cycles)
        sta m_delay
        lda #$ff
        sta m_delay+1
        lda td_var
        sec
        sbc #3
        sta td_var
        ldy #2
@nops:  lda td_var
        lsr                 ; td/2 (td is even here)
        tax
        beq @lda
@lp:    lda #$ea
        sta m_delay,y
        iny
        dex
        bne @lp
@lda:   lda #$ad            ; lda $dd00
        sta m_delay,y
        iny
        lda #$00
        sta m_delay,y
        iny
        lda #$dd
        sta m_delay,y
        iny
        lda dl_cur
        lsr
        bcc @tailbit        ; even -> bit $ff (3 cycles)
        lda #$ea            ; odd -> nop nop (4 cycles)
        sta m_delay,y
        iny
        lda #$ea
        sta m_delay,y
        rts
@tailbit:
        lda #$24
        sta m_delay,y
        iny
        lda #$ff
        sta m_delay,y
        rts

; r_index: Y = byte offset of r(fl,c6,dl) in r_tab
r_index:
        lda fl_idx
        asl
        asl
        asl
        asl
        sta tmp             ; 16*fl
        asl
        clc
        adc tmp             ; 48*fl
        sta tmp
        lda c6_idx
        asl
        asl
        asl
        sta tmp2            ; 8*c6
        asl                 ; 16*c6
        clc
        adc tmp2            ; 24*c6
        clc
        adc tmp
        sta tmp
        lda dli
        asl                 ; 2*dli
        sec
        sbc #4              ; 2*(dli-2)
        clc
        adc tmp
        tay
        rts

; r_fetch: r_cur = r(fl,c6,i_cur)
r_fetch:
        lda i_cur           ; the table loop indexes with i_cur, not dl_cur
        sta dli
        jsr r_index
        lda r_tab,y
        sta r_cur
        lda r_tab+1,y
        sta r_cur+1
        rts

; rd_store: rd_tab[i_cur] = rd_cur
rd_store:
        lda i_cur
        asl
        tay
        lda rd_cur
        sta rd_tab,y
        lda rd_cur+1
        sta rd_tab+1,y
        rts

; a_store: a_tab[c_cnt] = rd_cur-3000 : c_cnt = c_cnt+1
; (offset keeps the 32-bit sum small; calc_avg adds 3000 back)
a_store:
        lda c_cnt
        asl
        tay
        sec
        lda rd_cur
        sbc #<$0bb8         ; -3000
        sta a_tab,y
        lda rd_cur+1
        sbc #>$0bb8
        sta a_tab+1,y
        inc c_cnt
        rts

; prstr: A/Y = pointer to zero-terminated string
prstr:  sta ptr1
        sty ptr1+1
        ldy #0
@loop:  lda (ptr1),y
        beq @done
        jsr chrout
        iny
        bne @loop
@done:  rts

; prnum16: BASIC-style number print: leading space + unsigned decimal
prnum16:
        pha
        tya
        pha
        lda #' '
        jsr chrout
        pla
        tay
        pla
        jsr prdec16
        lda #' '            ; BASIC prints a trailing space after a number
        jsr chrout
        rts

; prdec16: print unsigned AY as decimal (leading zeros suppressed)
prdec16:
        sta num16
        sty num16+1
        lda #0
        sta sigf
        ldx #0              ; power index
@dig:   lda #$30            ; digit count -> ascii
        sta cnt1
@sub:   lda num16
        cmp pwlo,x
        lda num16+1
        sbc pwhi,x
        bcc @emit
        lda num16
        sbc pwlo,x
        sta num16
        lda num16+1
        sbc pwhi,x
        sta num16+1
        inc cnt1
        jmp @sub
@emit:  lda cnt1
        cmp #$31           ; digit > '0'?
        bcs @pr
        cpx #4             ; '0': last digit always printed
        beq @pr
        lda sigf
        beq @skip
@pr:    lda #1
        sta sigf
        lda cnt1
        jsr chrout
@skip:  inx
        cpx #5
        bne @dig
        rts

; div510: num16 = (num16+5)/10  (round thousandths -> hundredths)
div510: clc
        lda num16
        adc #5
        sta num16
        lda num16+1
        adc #0
        sta num16+1
        lda #10
        jsr div16by
        lda q16
        sta num16
        lda q16+1
        sta num16+1
        rts

; div16by: num16 / A (10 or 100) -> q16, rem16
div16by:
        sta divsor
        lda #0
        sta q16
        sta q16+1
@lp:    lda num16
        cmp divsor
        lda num16+1
        sbc #0
        bcc @done
        sec
        lda num16
        sbc divsor
        sta num16
        lda num16+1
        sbc #0
        sta num16+1
        inc q16
        bne @lp
        inc q16+1
        jmp @lp
@done:  lda num16
        sta rem16
        rts

; pr_hund: print unsigned num16 (=value*100) as N / .NN / N.N / N.NN
; (BASIC floating point print: trailing zeros trimmed, no leading zero
; before the decimal point for values < 1)
pr_hund:
        lda #100
        jsr div16by
        lda q16
        bne @intp
        lda q16+1
        bne @intp
        lda rem16
        bne @frac           ; 0.xx -> ".xx"
        lda #'0'
        jsr chrout          ; 0.00 -> "0"
        rts
@intp:  lda q16
        ldy q16+1
        jsr prdec16
@frac:  lda rem16
        beq @done
        lda #'.'
        jsr chrout
        lda rem16
        ldx #$2f
@t:     inx
        sec
        sbc #10
        bcs @t
        adc #$3a            ; ones char (carry clear here)
        sta ones_ch
        txa
        jsr chrout          ; tens char
        lda ones_ch
        cmp #$30
        beq @done           ; trailing zero trimmed
        jsr chrout
@done:  rts

; pr_h: num16 = signed hundredths -> BASIC-style int(x*100+0.5)/100
pr_h:   lda num16+1
        bpl @pos
        lda #'-'
        jsr chrout
        sec
        lda #0
        sbc num16
        sta num16
        lda #0
        sbc num16+1
        sta num16+1
        jmp @go
@pos:   lda #' '
        jsr chrout
@go:    jmp pr_hund

; pr_fp: num16 = signed thousandths -> BASIC-style int(x*100+0.5)/100
pr_fp:  lda num16+1
        bpl @pos
        lda #'-'
        jsr chrout
        sec
        lda #0
        sbc num16
        sta num16
        lda #0
        sbc num16+1
        sta num16+1
        jmp @go
@pos:   lda #' '
        jsr chrout
@go:    jsr div510
        jmp pr_hund

; pr_lus: print "{left}us"
pr_lus: lda #'u'
        jsr chrout
        lda #'s'
        jsr chrout
        rts

; pr_cyc: A = ra_tab cell (0-3); print average in C64 cycles
pr_cyc: asl
        tay
        lda ra_tab,y
        sta num16
        lda ra_tab+1,y
        sta num16+1
        jmp pr_fp

; pr_us: A = ra_tab cell; print average in microseconds + "{left}us"
pr_us:  pha
        asl
        tay
        lda ra_tab,y
        sta num16
        lda ra_tab+1,y
        sta num16+1
        jsr calc_us
        lda us_cur
        sta num16
        lda us_cur+1
        sta num16+1
        jsr pr_fp
        jsr pr_lus
        pla
        rts

; tabcol: A = target column (BASIC comma emulation)
tabcol: cmp curcol
        bcc @done
        beq @done
        pha
        lda #crsr_r
        jsr chrout
        pla
        jmp tabcol
@done:  rts

; pause: print "? " and wait for RETURN (mimics input a$)
pause:  lda #'?'
        jsr chrout
        lda #' '
        jsr chrout
        rts                 ; no-op wait (test harness has no keyboard)

; calc_rd: num40 = i_cur*40000000 - r_cur*27587 (exact, signed 32-bit)
;          rd_cur = round(num40/40000)   thousandths (storage/average)
;          h1_cur = round(num40/400000)  hundredths  (display, exact int())
calc_rd:
        lda i_cur           ; mul_out = i*40000000 (via i*4000 * 10000)
        sta mul_a
        lda #0
        sta mul_a+1
        lda #<4000
        sta mul_b
        lda #>4000
        sta mul_b+1
        jsr umul
        lda mul_out         ; mul_a = i*4000 (fits 16 bits)
        sta mul_a
        lda mul_out+1
        sta mul_a+1
        lda #<10000
        sta mul_b
        lda #>10000
        sta mul_b+1
        jsr umul
        lda mul_out         ; num32 = i*400000
        sta num32
        lda mul_out+1
        sta num32+1
        lda mul_out+2
        sta num32+2
        lda mul_out+3
        sta num32+3
        lda r_cur           ; mul_out = r*27587
        sta mul_a
        lda r_cur+1
        sta mul_a+1
        lda #<fqcyc
        sta mul_b
        lda #>fqcyc
        sta mul_b+1
        jsr umul
        sec                 ; num32 -= r*27587
        lda num32
        sbc mul_out
        sta num32
        lda num32+1
        sbc mul_out+1
        sta num32+1
        lda num32+2
        sbc mul_out+2
        sta num32+2
        lda num32+3
        sbc mul_out+3
        sta num32+3
        lda num32+3         ; sign
        and #$80
        sta negf
        beq @pos
        sec
        lda #0
        sbc num32
        sta num32
        lda #0
        sbc num32+1
        sta num32+1
        lda #0
        sbc num32+2
        sta num32+2
        lda #0
        sbc num32+3
        sta num32+3
@pos:   lda num32           ; rd_cur = (|num40|+20000)/40000  (thousandths)
        clc
        adc #<20000
        sta div_dend
        lda num32+1
        adc #>20000
        sta div_dend+1
        lda num32+2
        adc #0
        sta div_dend+2
        lda num32+3
        adc #0
        sta div_dend+3
        lda #$40            ; 40000
        sta div_dsor
        lda #$9c
        sta div_dsor+1
        lda #0
        sta div_dsor+2
        sta div_dsor+3
        jsr udiv32
        lda div_quot
        sta rd_cur
        lda div_quot+1
        sta rd_cur+1
        lda num32           ; h1_cur = (|num40|+200000)/400000 (hundredths)
        clc
        adc #<200000
        sta div_dend
        lda num32+1
        adc #>200000
        sta div_dend+1
        lda num32+2
        adc #$03            ; 200000 = $030D40
        sta div_dend+2
        lda num32+3
        adc #0
        sta div_dend+3
        lda #$80            ; 400000 = $061A80
        sta div_dsor
        lda #$1a
        sta div_dsor+1
        lda #$06
        sta div_dsor+2
        lda #$00
        sta div_dsor+3
        jsr udiv32
        lda div_quot
        sta h1_cur
        lda div_quot+1
        sta h1_cur+1
        lda num32           ; h2_cur = (|num40|+197050)/394100
        clc                 ; = int(rd/fq*100+0.5) hundredths (exact)
        adc #$ba            ; 197050 = $0301BA
        sta div_dend
        lda num32+1
        adc #$01
        sta div_dend+1
        lda num32+2
        adc #$03
        sta div_dend+2
        lda num32+3
        adc #$00
        sta div_dend+3
        lda #$74            ; 394100 = $060374
        sta div_dsor
        lda #$03
        sta div_dsor+1
        lda #$06
        sta div_dsor+2
        lda #$00
        sta div_dsor+3
        jsr udiv32
        lda div_quot
        sta h2_cur
        lda div_quot+1
        sta h2_cur+1
        lda negf            ; reapply sign
        beq @dne
        sec
        lda #0
        sbc rd_cur
        sta rd_cur
        lda #0
        sbc rd_cur+1
        sta rd_cur+1
        sec
        lda #0
        sbc h1_cur
        sta h1_cur
        lda #0
        sbc h1_cur+1
        sta h1_cur+1
        sec
        lda #0
        sbc h2_cur
        sta h2_cur
        lda #0
        sbc h2_cur+1
        sta h2_cur+1
@dne:   rts

; calc_us: us_cur = (num16*40000+1970)/3941, signed (thousandths of us)
calc_us:
        lda num16+1
        and #$80
        sta negf
        beq @pos
        sec
        lda #0
        sbc num16
        sta rd_tmp
        lda #0
        sbc num16+1
        sta rd_tmp+1
        jmp @m
@pos:   lda num16
        sta rd_tmp
        lda num16+1
        sta rd_tmp+1
@m:     lda rd_tmp
        sta mul_a
        lda rd_tmp+1
        sta mul_a+1
        lda #<qnum
        sta mul_b
        lda #>qnum
        sta mul_b+1
        jsr umul
        clc
        lda mul_out
        adc #<1970
        sta div_dend
        lda mul_out+1
        adc #>1970
        sta div_dend+1
        lda mul_out+2
        adc #0
        sta div_dend+2
        lda mul_out+3
        adc #0
        sta div_dend+3
        lda #<qden
        sta div_dsor
        lda #>qden
        sta div_dsor+1
        lda #0
        sta div_dsor+2
        sta div_dsor+3
        jsr udiv32
        lda div_quot
        sta us_cur
        lda div_quot+1
        sta us_cur+1
        lda negf
        beq @dne
        sec
        lda #0
        sbc us_cur
        sta us_cur
        lda #0
        sbc us_cur+1
        sta us_cur+1
@dne:   rts

; calc_x4: x_tab[c] = int(ra_tab[c]/fq*100+0.5) hundredths (0 if negative)
calc_x4:
        lda #0
        sta xloop
@xl:    lda xloop
        asl
        tay
        lda ra_tab,y
        sta num16
        lda ra_tab+1,y
        sta num16+1
        lda num16+1
        and #$80
        sta negf
        beq @pos
        sec                 ; |ra_th| -> rd_tmp
        lda #0
        sbc num16
        sta rd_tmp
        lda #0
        sbc num16+1
        sta rd_tmp+1
        jmp @m
@pos:   lda num16
        sta rd_tmp
        lda num16+1
        sta rd_tmp+1
@m:     lda rd_tmp          ; rd_tmp*4000
        sta mul_a
        lda rd_tmp+1
        sta mul_a+1
        lda #<4000
        sta mul_b
        lda #>4000
        sta mul_b+1
        jsr umul
        lda mul_out         ; +19705  ($4CF9)
        clc
        adc #$f9
        sta div_dend
        lda mul_out+1
        adc #$4c
        sta div_dend+1
        lda mul_out+2
        adc #$00
        sta div_dend+2
        lda mul_out+3
        adc #$00
        sta div_dend+3
        lda #$f2            ; 39410
        sta div_dsor
        lda #$99
        sta div_dsor+1
        lda #0
        sta div_dsor+2
        sta div_dsor+3
        jsr udiv32
        lda negf
        beq @store
        lda #0              ; negative average -> "<min" anyway
        sta div_quot
        sta div_quot+1
@store: lda xloop
        asl
        tay
        lda div_quot
        sta x_tab,y
        lda div_quot+1
        sta x_tab+1,y
        inc xloop
        lda xloop
        cmp #4
        bcc @xl2
        rts
@xl2:   jmp @xl

; calc_avg: ra_th = sum(a_tab[0..c_cnt-1]) / c_cnt (signed)
calc_avg:
        lda #0
        sta sum32
        sta sum32+1
        sta sum32+2
        sta sum32+3
        ldx #0
@sum:   lda a_tab,x         ; sign-extend addend into tmp32
        sta tmp32
        lda a_tab+1,x
        sta tmp32+1
        and #$80
        beq @pos
        lda #$ff
        bne @set
@pos:   lda #0
@set:   sta tmp32+2
        sta tmp32+3
        clc
        lda sum32
        adc tmp32
        sta sum32
        lda sum32+1
        adc tmp32+1
        sta sum32+1
        lda sum32+2
        adc tmp32+2
        sta sum32+2
        lda sum32+3
        adc tmp32+3
        sta sum32+3
        inx
        inx
        txa
        lsr
        cmp c_cnt
        bcc @sum
        lda sum32+3         ; sign of sum
        and #$80
        sta negf
        beq @dpos
        sec
        lda #0
        sbc sum32
        sta div_dend
        lda #0
        sbc sum32+1
        sta div_dend+1
        lda #0
        sbc sum32+2
        sta div_dend+2
        lda #0
        sbc sum32+3
        sta div_dend+3
        jmp @rnd
@dpos:  lda sum32
        sta div_dend
        lda sum32+1
        sta div_dend+1
        lda sum32+2
        sta div_dend+2
        lda sum32+3
        sta div_dend+3
@rnd:   lda c_cnt           ; round: abs(sum) += c_cnt/2
        lsr
        clc
        adc div_dend
        sta div_dend
        lda #0
        adc div_dend+1
        sta div_dend+1
        lda c_cnt
        sta div_dsor
        lda #0
        sta div_dsor+1
        sta div_dsor+2
        sta div_dsor+3
        jsr udiv32
        lda div_quot
        sta ra_th
        lda div_quot+1
        sta ra_th+1
        lda negf
        beq @dne
        sec
        lda #0
        sbc ra_th
        sta ra_th
        lda #0
        sbc ra_th+1
        sta ra_th+1
@dne:   clc                 ; undo -3000 offset: ra_th += 3000
        lda ra_th
        adc #$b8
        sta ra_th
        lda ra_th+1
        adc #$0b
        sta ra_th+1
        rts

; umul: mul_a * mul_b -> mul_out (32-bit)
umul:   lda #0
        sta mul_out
        sta mul_out+1
        sta mul_out+2
        sta mul_out+3
        ldx #16
@lp:    lsr mul_a+1
        ror mul_a
        bcc @nadd
        clc
        lda mul_b
        adc mul_out+2
        sta mul_out+2
        lda mul_b+1
        adc mul_out+3
        sta mul_out+3
@nadd:  ror mul_out+3
        ror mul_out+2
        ror mul_out+1
        ror mul_out
        dex
        bne @lp
        rts

; udiv32: div_dend / div_dsor -> div_quot, div_rem (all 32-bit unsigned)
udiv32: lda #0
        sta div_rem
        sta div_rem+1
        sta div_rem+2
        sta div_rem+3
        sta div_quot
        sta div_quot+1
        sta div_quot+2
        sta div_quot+3
        ldx #32
@lp:    asl div_dend
        rol div_dend+1
        rol div_dend+2
        rol div_dend+3
        rol div_rem
        rol div_rem+1
        rol div_rem+2
        rol div_rem+3
        lda div_rem
        cmp div_dsor
        lda div_rem+1
        sbc div_dsor+1
        lda div_rem+2
        sbc div_dsor+2
        lda div_rem+3
        sbc div_dsor+3
        bcc @nsub
        lda div_rem
        sbc div_dsor
        sta div_rem
        lda div_rem+1
        sbc div_dsor+1
        sta div_rem+1
        lda div_rem+2
        sbc div_dsor+2
        sta div_rem+2
        lda div_rem+3
        sbc div_dsor+3
        sta div_rem+3
@nsub:  rol div_quot
        rol div_quot+1
        rol div_quot+2
        rol div_quot+3
        dex
        bne @lp
        rts

; =========================================================================
; strings (PETSCII, zero terminated)
; =========================================================================
s_u0m0: !text "u0>m0"
s_me:   !text "m-e",$00,$05
s_ui:   !text "ui"
s_wait: !byte down,down
        !text " Please wait",$0d,0
s_flop: !byte down,down
        !text " Floppy: ",0
s_c64l: !text " C64:    ",0
s_edge: !text " edge",$0d,0
s_fall: !text "falling",0
s_rise: !text "rising",0
s_dcyc: !text " delay cycles:",0
s_presp:!text "pos.responses:",0
s_pct:  !byte crsr_l,'%', $0d,0
s_ret:  !byte down,down,up
        !text " <Return>",0
s_devnp:!text $0d,"?device not present",$0d,0

s_row2: !text "  ",0
s_relhdr1:
        !byte down
        !text "                    relative  relative",$0d,0
s_relhdr2:
        !text " Delay              delay     delay",$0d,0
s_relhdr3:
        !text " in C64  positive   in C64    in micro-",$0d,0
s_relhdr4:
        !text " cycles  responses  cycles    seconds",$0d,0
s_avgh: !byte $0d
        !text " Average relativedelay:",$0d,0
s_sp:   !text " ",0
s_c64cyc:
        !text " C64-cycles",$0d,0
s_usec: !text " microseconds",$0d,0

s_p1h:  !byte clrscr
        !text "results (1/2)",$0d,0
s_p1a:  !byte $0d
        !text "                      Floppy edge",$0d,0
s_p1b:  !text "                     in C64-cycles",$0d,0
s_p1c:  !byte $0d,$0d
        !text "                      Floppy edge",$0d,0
s_p1d:  !text "                     in microseconds",$0d,0
s_p1e:  !byte $0d
        !text " Falling edge = IEC-bus 5V -> 0V",$0d,0
s_p1f:  !text " Rising edge  = IEC-bus 0V -> 5V",$0d,0
s_p1g:  !byte $0d
        !text " Edge = what the C64/floppy makes",$0d,0
s_p1h2: !text "        (not what it reacts to)",$0d,0
s_p1r:  !byte down,down,down,down,up
        !text " <Return>",0
s_p1r1: !text " C64 falling edge",0
s_p1r2: !text " C64 rising edge",0

s_p2h:  !byte clrscr
        !text "results (2/2)",$0d,0
s_p2a:  !byte $0d
        !text "    C64=fal  C64=ris",crsr_r," C64=fal  C64=ris",$0d,0
s_p2b:  !text "     41=fal   41=fal   41=ris   41=ris",$0d,0
s_p2c:  !text " a   2.86us   2.83us   2.89us   2.86us",$0d,0
s_p2d:  !text " b   2.97us   2.94us   3.01us   2.98us",$0d,0
s_p2e:  !text " c   2.75us   2.72us   2.77us   2.74us",$0d,0
s_p2f:  !text " d   2.99us   2.85us   2.97us   2.83us",$0d,0
s_p2g:  !text " e   3.00us   2.94us   3.01us   2.94us",$0d,0
s_xr:   !text " x  ",0
s_min:  !text "<min     ",0
s_max:  !text ">max     ",0
s_ok:   !text "ok       ",0
s_leg1: !text " a= 1541-2        1. of 1 device",$0d,0
s_leg2: !text " b= 1541old       1. of 1 device",$0d,0
s_leg3: !text " c= 1571ext       1. of 1 device",$0d,0
s_leg4: !text " d= 1541-2        1. of 5 devices",$0d,0
s_leg5: !text " e= 1541-2        5. of 5 devices",$0d,0
s_leg6: !text " x= tested device",$0d,0
s_p2r:  !byte down,down,up
        !text " <Return>",0

; =========================================================================
; variables
; =========================================================================
fl_idx  !byte 0
c6_idx  !byte 0
dl_cur  !byte 0
dli     !byte 0
i_cur   !byte 0
c_cnt   !byte 0
td_var  !byte 0
cnt1    !byte 0
sigf    !byte 0
negf    !byte 0
ones_ch !byte 0
divsor  !byte 0
rem16   !byte 0
tmp     !byte 0
tmp2    !byte 0
bc_save !byte 0
xloop   !byte 0
upidx   !byte 0
rttry   !word 0
dbg1    !byte 0
dbg2    !byte 0
dbg3    !byte 0
dbg4    !byte 0
h2_cur  !word 0
num16   !word 0
q16     !word 0
r_cur   !word 0
rd_cur  !word 0
h1_cur  !word 0
rd_tmp  !word 0
us_cur  !word 0
ra_th   !word 0
num32   !fill 4
mul_a   !word 0
mul_b   !word 0
mul_out !fill 4
div_dend!fill 4
div_dsor!fill 4
div_quot!fill 4
div_rem !fill 4
sum32   !fill 4
tmp32   !fill 4
mwbuf   !text "m-w"         ; +3..5: addr lo/hi, count; +6.. data
        !fill 42
r_tab   !fill 96            ; r(fl,c6,dl) words, 2*2*12
rd_tab  !fill 24            ; rd(0..11) words
a_tab   !fill 20            ; a(0..9) words
ra_tab  !fill 8             ; ra(fl,c6) words
x_tab   !fill 8             ; x1..x4 in hundredths
mi_tab: !byte $13,$10,$15,$12   ; 275,272,277,274  (lo bytes)
        !byte $01,$01,$01,$01    ; (hi bytes)
ma_tab: !byte $2c,$26,$2d,$2a   ; 300,294,301,298  (lo bytes)
        !byte $01,$01,$01,$01    ; (hi bytes)

pwlo:   !byte <10000,<1000,<100,<10,<1
pwhi:   !byte >10000,>1000,>100,>10,>1

; =========================================================================
; measurement routine (BASIC DATA 3140-3170, 71 bytes)
; ---------------------------------------------------------------------------
meas:
        sei
        ldx #$00
        stx $fd          ; result counter lo
        stx $fe          ; result counter hi
        tax              ; a = 0
        eor #$10         ; a = $10
        tay              ; y = $10
mloop:  stx dd00         ; <- loop start
m_delay:
!fill 12, $ea            ; delay area, patched at runtime (12 nops)
        sty dd00
        and #$80
m_branch:
        beq mcount       ; <- patched: beq/bne (floppy edge)
        bit $00
        nop
        nop
        nop
        nop
        jmp mborder
mcount: inc $fd
        beq minh
        bit $00
        jmp mborder
minh:   inc $fe
mborder:
        inc d020
        inc $fb
        beq mfcl
        nop
        nop
        nop
        jmp mloop
mfcl:   inc $fc
        bne mloop
        rts

; =========================================================================
; drive code buffer (BASIC DATA 3180-3190, 39 bytes)
; Uploaded to 1541 RAM at $0500 via M-W; this copy is patched per combo.
; ---------------------------------------------------------------------------
drv:
        sei
        lda $1800        ; 1541 IEC port
        and #$f5
d_ora:
        ora #$00         ; <- patched: ora #$00/#$02 (floppy edge)
        tax
        eor #$02
        tay
dloop:  stx $1800        ; <- loop start
        lda #$04
dwait:  bit $1800
d_branch:
        beq dwait        ; <- patched: beq/bne (c64 edge)
        sty $1800
        lda $1c00
        eor #$08
        sta $1c00
        lda $1800
        bpl dloop
        rts

