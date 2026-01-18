# RP2040 Commodore 1541 Floppy Drive Emulator

A Commodore 1541 floppy drive emulator that runs on the Raspberry Pi Pico (RP2040).

## Features

- Full 1541 drive emulation via IEC serial bus
- Supports D64 disk images
- Open-collector GPIO implementation for proper IEC bus signaling

## GPIO Pin Assignments

| GPIO Pin | IEC Line | Description        |
|----------|----------|--------------------|
| 2        | DATA     | Data line (bidirectional) |
| 3        | CLK      | Clock line (bidirectional) |
| 4        | ATN      | Attention line (input from C64) |
| 5        | SRQ      | Service Request (reserved, input only) |
| 6        | RESET    | Reset line |

## Build Instructions

```bash
cd rp2040
./build.sh
```
