# C1541 fork

## Building

1. `mkdir fips-workspace && cd fips-workspace`
2. `git clone https://github.com/floooh/chips-test`
3. `cd chips-test` and change `git: https://github.com/floooh/chips` in `fips.yml` to `git: git@github.com:c1570/chips.git`
4. `./fips make` to build
5. `../fips-deploy/chips-test/linux-make-debug/c64-ui -c1541` to run the emulator

## Technical Background Information

* [Ultimate Commodore 1541 Drive Talk](https://www.youtube.com/watch?v=_1jXExwse08)
* [Hardware-Aufbau der 1541](https://www.c64-wiki.de/wiki/Hardware-Aufbau_der_1541)
* [Gate Array](/docs/PN325572.png)
* [ROM disassembly](https://g3sl.github.io/c1541rom.html)
* [G64 documentation](http://www.unusedino.de/ec64/technical/formats/g64.html)
* Denise source code: [mechanics.cpp](https://bitbucket.org/piciji/denise/src/master/emulation/libc64/disk/drive/mechanics.cpp) - see via2.ca1In and cpu.triggerSO - and [opcodes.cpp](https://bitbucket.org/piciji/denise/src/master/emulation/libc64/disk/cpu/opcodes.cpp) - see soBlock

## Milestones

1. Get drive CPU to reset and run to idle ($ec12..$ec2d)
2. Connect drive and host (reading error channel from BASIC should return `73, CBM DOS V2.6 1541, 0, 0`)
3. Read directory from GCR data (i.e., keep passing [track 18 GCR data](/docs/1541_test_demo_track18gcr.h) to VIA, handle SYNC and SO CPU line, needs m6502.h changes, see Denise source)
4. Read full disk from G64 image (implement stepper motor)
5. Read full disk from D64 image (on the fly encoding from D64 to GCR)
6. Write support (this is complicated)

# chips

[![Build Status](https://github.com/floooh/chips/workflows/build_and_test/badge.svg)](https://github.com/floooh/chips/actions)

A toolbox of 8-bit chip-emulators, helper code and complete embeddable
system emulators in dependency-free C headers (a subset of C99 that
compiles on gcc, clang and cl.exe).

Tests and example code is in a separate repo: https://github.com/floooh/chips-test

The example emulators, compiled to WebAssembly: https://floooh.github.io/tiny8bit/

For schematics, manuals and research material, see: https://github.com/floooh/emu-info

The USP of the chip emulators is that they communicate with the outside world through
a 'pin bit mask': A 'tick' function takes an uint64_t as input where the bits
represent the chip's in/out pins, the tick function inspects the pin
bits, computes one tick, and returns a (potentially modified) pin bit mask.

A complete emulated computer then more or less just wires those chip emulators
together just like on a breadboard.

In reality, most emulators are not quite as 'pure' (as this would affect performance
too much or complicate the emulation): some chip emulators have a small number
of callback functions and the adress decoding in the system emulators often
take shortcuts instead of simulating the actual address decoding chips
(with one exception: the lc80 emulator).
