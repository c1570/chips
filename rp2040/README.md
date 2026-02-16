# RP2 Commodore 1541 Floppy Drive Emulator

A Commodore 1541 floppy drive emulator that runs on the Raspberry Pi Pico (RP2040/2350).

## Features

- Work in progress
- Full 1541 drive emulation via IEC serial bus
- Supports D64 and G64 disk images
- Open-collector GPIO implementation for proper IEC bus signaling

## GPIO Pin Assignments

| GPIO Pin | IEC Line | Description            |
|----------|----------|------------------------|
| 2        | ATN      | Attention line (input) |
| 3        | CLK      | Clock line (bidir)     |
| 4        | DATA     | Data line (bidir)      |
| 5        | RESET    | Reset line (input)     |
| 29       | SRQ      | Service Request (res)  |

## Build Instructions

```bash
cd rp2040
./build.sh
```
