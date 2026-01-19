#!/usr/bin/env node
/**
 * RP2040 C1541 Test Runner
 *
 * Runs the RP2040 C1541 firmware in rp2040js and interfaces with C64 emulator
 */

const RP_MHZ = 125;

import { RP2040, GPIOPinState } from './rp2040js/dist/esm/index.js';
import { bootromB1 } from './rp2040js/demo/bootrom.js';
import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';
import { decodeBlock } from 'uf2';
import { createRequire } from 'module';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);
const koffi = require('koffi');

// UF2 loading
const FLASH_START_ADDRESS = 0x10000000;
function loadUF2(filename, rp2040) {
  const file = fs.openSync(filename, 'r');
  const buffer = new Uint8Array(512);
  while (fs.readSync(file, buffer) === buffer.length) {
    const block = decodeBlock(buffer);
    const { flashAddress, payload } = block;
    rp2040.flash.set(payload, flashAddress - FLASH_START_ADDRESS);
  }
  fs.closeSync(file);
}

const FIRMWARE_PATH = `${__dirname}/../rp2040/build/c1541.uf2`;
const C64_LIB_PATH = `${__dirname}/libc64_emulation.so`;
const INITIAL_PC = 0x10000000;

// IEC GPIO pins on RP2040
const IEC_GPIO_DATA   = 2;
const IEC_GPIO_CLK    = 3;
const IEC_GPIO_ATN    = 4;
const IEC_GPIO_SRQ    = 5;
const IEC_GPIO_RESET  = 6;

// IEC line definitions (must match iecbus.h)
const IECLINE_DATA  = 1 << 0;
const IECLINE_CLK   = 1 << 1;
const IECLINE_ATN   = 1 << 2;
const IECLINE_SRQIN = 1 << 3;
const IECLINE_RESET = 1 << 4;

// Load C64 library
const lib = koffi.load(C64_LIB_PATH);

const c64_init = lib.func('void c64_emulation_init()');
const c64_tick = lib.func('void c64_emulation_tick()');
const c64_set_iec = lib.func('void c64_set_iec_gpio(uint8_t state)');
const c64_get_iec = lib.func('uint8_t c64_get_iec_bus()');
const c64_get_tick_count = lib.func('uint64_t c64_get_tick_count()');
const c64_print_tick_count = lib.func('void c64_print_tick_count()');
const c64_print_screen = lib.func('void c64_print_screen()');

// Initialize C64
console.log('Initializing C64 emulator...');
c64_init();

// Initialize RP2040
console.log('Initializing RP2040...');
const mcu = new RP2040();
mcu.loadBootrom(bootromB1);

let doTickC64 = false;

mcu.onTrace = function(coreNumber, pc, tag) {
  if(tag == "tick ") {
    doTickC64 = true;
  } else {
    console.log(`${mcu.cycles} PC 0x${pc.toString(16)} tag ${tag}`);
  }
}

function getOffsetForVariable(var_name) {
  const filename = FIRMWARE_PATH.replace(".uf2", ".elf.map");
  const content = fs.readFileSync(filename, 'utf-8');
  const search = var_name.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
  const re = new RegExp(search + ".*\n *(0x[0-9a-f]+) ");
  const res = re.exec(content);
  if(res == null) throw new Error(`Could not find offset of variable ${var_name} in map file ${filename}`);
  return parseInt(res[1]);
}

// Load C1541 firmware
if (fs.existsSync(FIRMWARE_PATH)) {
  console.log(`Loading firmware from ${FIRMWARE_PATH}`);
  loadUF2(FIRMWARE_PATH, mcu);
  console.log('Firmware loaded successfully');
} else {
  console.error(`Firmware not found at ${FIRMWARE_PATH}`);
  console.error('Please build the firmware first: cd rp2040 && ./build.sh');
  process.exit(1);
}

// Set up UART output
mcu.uart[0].onByte = (value) => {
  process.stdout.write(new Uint8Array([value]));
};

// Set initial PC
mcu.core0.PC = INITIAL_PC;

// GPIO tracking for IEC signals
let lastIecState = 0xFF;

function tickC64() {
  // Read RP2040 IEC GPIO outputs
  // GPIO value: Low=active (0), High=inactive (1)
  // IEC format uses: 0=active, 1=inactive - same as GPIO direction logic!
  const dataOut  = mcu.gpio[IEC_GPIO_DATA].value !== GPIOPinState.Low;
  const clkOut   = mcu.gpio[IEC_GPIO_CLK].value !== GPIOPinState.Low;
  const atnOut   = mcu.gpio[IEC_GPIO_ATN].value !== GPIOPinState.Low;
  const resetOut = mcu.gpio[IEC_GPIO_RESET].value !== GPIOPinState.Low;

  // Build IEC state byte (format matches IECLINE_* bits: 0=DATA, 1=CLK, 2=ATN, 4=RESET)
  let iecState = 0xFF;  // All inactive (high) by default
  if (!dataOut)  iecState &= ~IECLINE_DATA;   // Bit 0
  if (!clkOut)   iecState &= ~IECLINE_CLK;    // Bit 1
  if (!atnOut)   iecState &= ~IECLINE_ATN;    // Bit 2
  if (!resetOut) iecState &= ~IECLINE_RESET;  // Bit 4

  // Send RP2040's IEC signals to C64
  if (iecState !== lastIecState) {
    c64_set_iec(iecState);
    lastIecState = iecState;
  }

  c64_tick();

  // Get combined IEC bus state (C64 + RP2040)
  const busState = c64_get_iec();

  // Apply IEC bus state to RP2040 GPIO inputs
  mcu.gpio[IEC_GPIO_DATA].setInputValue((busState & IECLINE_DATA) !== 0);
  mcu.gpio[IEC_GPIO_CLK].setInputValue((busState & IECLINE_CLK) !== 0);
  mcu.gpio[IEC_GPIO_ATN].setInputValue((busState & IECLINE_ATN) !== 0);
  mcu.gpio[IEC_GPIO_RESET].setInputValue((busState & IECLINE_RESET) !== 0);
}

let frameCount = 0;

function runEmulation() {
  const startTime = Date.now();

  // Run RP2040 for one frame worth of cycles
  const targetCycles = 125000000 / 20;
  let cyclesRun = 0;
  let c64TickCount = 0;

  while (cyclesRun < targetCycles) {
    const startCycles = mcu.cycles;
    mcu.step();
    const elapsed = mcu.cycles - startCycles;

    if (doTickC64) {
      tickC64();
      c64TickCount++;
      doTickC64 = false;
    }

    cyclesRun += elapsed;
  }

  if (frameCount % 10 === 0) {
    c64_print_screen();
    console.log(`${(cyclesRun/c64TickCount)>>0} RP2 cycles per C64 µs`);
    console.log(`Motor: ${mcu.gpio[8].value}  LED: ${mcu.gpio[25].value}`);
  }

  frameCount++;

  setTimeout(runEmulation);
}

console.log('Starting C64 + RP2040 C1541 emulation...');
console.log('Press Ctrl+C to stop');

setTimeout(runEmulation);
