; ============================================================
; C64 Cartridge — B-R (Block-Read) Sector Read Demo
;
; This code is placed at $8000 in C64 RAM.  On power-on / reset,
; the KERNAL cold-start sequence at $FCE2 checks bytes
; $8004–$8008 for the "CBM80" signature ($C3 $C2 $CD $38 $30).
; If found, it executes  JMP ($8000)  — entering our cartridge.
;
; The cartridge sends a "B-R" (block-read) command to the
; Commodore 1541 floppy drive to read track 18, sector 0
; (the BAM / directory header block), then dumps the 256-byte
; result to the screen as hexadecimal.
;
; The 1541 DOS parses B-R parameters as ASCII DECIMAL digits
; (not binary CHR$ bytes).  The format is:
;
;       B-R:<channel>,<drive>,<track>,<sector>
;
; Assemble:  acme -f plain -o cart-br-read.bin cart-br-read.asm
; ============================================================

; --- KERNAL jump-table routines ---
SETNAM  = $FFBD         ; A=len  X:Y=filename ptr
SETLFS  = $FFBA         ; A=LA  X=dev  Y=SA
OPEN    = $FFC0         ; open logical file (carry set on error)
CHKIN   = $FFC6         ; select input from logical file in X
CLRCHN  = $FFCC         ; restore default I/O channels
CHRIN   = $FFCF         ; read one byte from current input (A)
CHROUT  = $FFD2         ; write one byte to current output (BSOUT)

; --- KERNAL internal init entry points ---
; (identical to the sequence the cold-start uses at $FCF2–$FCFB)
K_IOINIT = $FDA3        ; initialise CIAs (incl. CIA2 for IEC bus) + VIC
K_RAMTAS = $FD50        ; clear RAM, set memory pointers
K_RESTOR = $FD15        ; restore default I/O vectors
K_SCRINI = $FF5B        ; initialise screen / editor

; --- Configuration ---
DEVICE  = 8             ; IEC device number (1541 = 8)
BUFCHN  = 2             ; buffer channel secondary address
CMDCHN  = 15            ; command channel secondary address
BUFADDR = $C000         ; where to store the 256-byte block

; ============================================================
; Cartridge header — read by KERNAL cold-start at $FCEC
; ============================================================
* = $8000
    !word cart_start              ; $8000–01: indirect-jump vector
    !byte $00, $00                ; $8002–03: unused
    !byte $C3, $C2, $CD, $38, $30 ; $8004–08: "CBM80" PETASCII

; ============================================================
; Entry point — KERNAL does  JMP ($8000)  to get here.
; At this point SEI is active and NOTHING is initialised,
; so we must replicate the normal boot init sequence first.
; ============================================================
cart_start:
    JSR K_IOINIT          ; init I/O (CIAs, VIC)
    JSR K_RAMTAS          ; clear zero-page / $0200–$03FF
    JSR K_RESTOR          ; set up KERNAL I/O vectors
    JSR K_SCRINI          ; init screen editor
    CLI                   ; enable interrupts (IRQ for keyboard etc.)

    ; --- Clear screen, print banner ---
    LDA #$93              ; PETASCII CLEAR
    JSR CHROUT
    LDX #0
print_banner:
    LDA banner,X
    BEQ banner_done
    JSR CHROUT
    INX
    BNE print_banner
banner_done:

    ; patched C1541 ROM boots fast, don't need delay here

    ; =====================================================
    ; Step 1:  OPEN 2,8,2,"#"
    ;          Allocate an internal buffer in the drive
    ; =====================================================
    LDA #1                ; filename length = 1
    LDX #<hash_char
    LDY #>hash_char
    JSR SETNAM
    LDA #BUFCHN           ; logical file 2
    LDX #DEVICE           ; device 8
    LDY #BUFCHN           ; secondary address 2
    JSR SETLFS
    JSR OPEN
    BCS open_error

    ; =====================================================
    ; Step 2:  OPEN 15,8,15,"B-R:2,0,18,0"
    ;          Opens the command channel AND sends the B-R
    ;          command (the "filename" IS the command).
    ; =====================================================
    LDA #(br_cmd_end - br_cmd)   ; command length
    LDX #<br_cmd
    LDY #>br_cmd
    JSR SETNAM
    LDA #CMDCHN           ; logical file 15
    LDX #DEVICE           ; device 8
    LDY #CMDCHN           ; secondary address 15
    JSR SETLFS
    JSR OPEN
    BCS open_error

    ; =====================================================
    ; Step 3:  Read 256 bytes from the buffer channel
    ;          GET#2  →  stored at $C000–$C0FF
    ; =====================================================
    LDX #BUFCHN
    JSR CHKIN             ; select input from logical file 2
    LDY #0
read_loop:
    JSR CHRIN             ; read one byte (returns in A)
    STA BUFADDR,Y         ; store in buffer
    INY
    BNE read_loop         ; Y wraps 255→0  ⇒  256 bytes done

    JSR CLRCHN            ; restore default input (keyboard)

    ; =====================================================
    ; Step 4:  Display the 256-byte block as hex on screen
    ; =====================================================
    JSR show_data

    ; Signal completion to the host emulator (poll $C100 for $FF)
    LDA #$FF
    STA $C100
forever:
    JMP forever


; --- Error handler (OPEN failed — device not ready?) ---
open_error:
    LDA #$EE            ; error marker
    STA $C100
    LDA #2              ; red border
    STA $D020
    JMP forever


; ===========================================================
; show_data — hex dump of BUFADDR … BUFADDR+255
;
; Prints 13 hex bytes per line (39 chars, fits 40-col screen).
; Zero-page variables idx/col are used as loop counters so that
; KERNAL CHROUT (which clobbers A/Y) cannot corrupt the state.
; ===========================================================
idx = $02               ; zero-page: buffer index (0–255)
col = $03               ; zero-page: column counter (0–12)

show_data:
    LDA #0
    STA idx
    STA col
.show_l:
    LDY idx
    LDA BUFADDR,Y        ; fetch byte from buffer
    JSR phex             ; print as 2 hex digits
    LDA #' '
    JSR CHROUT           ; space separator
    INC idx
    INC col
    LDA col
    CMP #13
    BNE .same_line
    LDA #$0D             ; newline every 13 bytes
    JSR CHROUT
    LDA #0
    STA col
.same_line:
    LDA idx
    BNE .show_l          ; idx wrapped to 0 → all 256 bytes done
    RTS


; ===========================================================
; phex — print accumulator as two hexadecimal digits
; (clobbers A and Y)
; ===========================================================
phex:
    PHA
    LSR                 ; high nibble → low
    LSR
    LSR
    LSR
    TAY
    LDA hex_tab,Y
    JSR CHROUT
    PLA
    AND #$0F             ; low nibble
    TAY
    LDA hex_tab,Y
    JMP CHROUT


; ===========================================================
; Data
; ===========================================================
delay_ctr:
    !byte 0

hex_tab:
    !text "0123456789ABCDEF"

hash_char:
    !text "#"

; B-R command — 1541 DOS parses parameters as ASCII decimal.
; Channel 2, drive 0, track 18, sector 0.
br_cmd:
    !text "B-R:2,0,18,0"
br_cmd_end:

banner:
    !byte $13            ; HOME cursor
    !text "B-R READ DEV8 T18 S0"
    !byte $0D, $0D, 0    ; two newlines, NUL terminator
