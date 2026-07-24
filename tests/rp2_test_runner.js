#!/usr/bin/env node
/**
 * RP2 C1541 Test Runner
 *
 * Runs the RP2 C1541 firmware in rp2350js and interfaces with C64 emulator
 */

const RP_MHZ = 125;

import { RP2350, GPIOPinState } from './rp2350js/dist/esm/index.js';
import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';
import { decodeBlock } from 'uf2';
import { createRequire } from 'module';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);
const koffi = require('koffi');

const FIRMWARE_PATH = `${__dirname}/../rp2040/build/c1541.uf2`;
const C64_LIB_PATH = `${__dirname}/libc64_emulation.so`;

// IEC GPIO pins on RP2
const IEC_GPIO_ATN    = 2;
const IEC_GPIO_CLK    = 3;
const IEC_GPIO_DATA   = 4;
const IEC_GPIO_RESET  = 5;
const IEC_GPIO_SRQ    = 29;

const RP2_DISK_CHANGE_PIN  = 8;
const RP2_MOTOR_STATUS_PIN = 24;
const RP2_LED_PIN          = 25;

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

// Initialize RP2
console.log('Initializing RP2...');
const mcu = new RP2350();
mcu.loadFirmware(FIRMWARE_PATH);

function getOffsetForVariable(var_name) {
  const filename = FIRMWARE_PATH.replace(".uf2", ".elf.map");
  const content = fs.readFileSync(filename, 'utf-8');
  const search = var_name.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
  const re = new RegExp(search + ".*\n *(0x[0-9a-f]+) ");
  const res = re.exec(content);
  if(res == null) throw new Error(`Could not find offset of variable ${var_name} in map file ${filename}`);
  return parseInt(res[1]);
}

// Set up UART output
mcu.uart[0].onByte = (value) => {
  process.stdout.write(new Uint8Array([value]));
};

const displayMotorAnim = [ "b", "d", "q", "p"];

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

function emu() {
    mcu.onTrace = function(coreNumber, pc, tag) {
        if(tag == "tick ") {
            c1541TickDone = true;
        } else {
            console.log(`${mcu.cycles} PC 0x${pc.toString(16)} tag ${tag}`);
        }
    }

    const c64Config = {
        pal: {
            ticksPerSecond: 985249,
            framesPerSecond: 25,
        },
        ntsc: {
            ticksPerSecond: 1022727,
            framesPerSecond: 30000 / 1001,
        }
    };

    const c64VideoModel = "pal";
    
    const c64TicksPerSecond = c64Config[c64VideoModel].ticksPerSecond;
    const c64FramesPerSecond = c64Config[c64VideoModel].framesPerSecond;

    const c1541TicksPerSecond = 1000000;

    const ticksPerVideoFrame = c64TicksPerSecond / c64FramesPerSecond;
    
    const tickC64ToC1541Ratio = c64TicksPerSecond / c1541TicksPerSecond;
    const c64TickDelta = (tickC64ToC1541Ratio > 1) ? 1 : tickC64ToC1541Ratio;
    const c1541TickDelta = (tickC64ToC1541Ratio > 1) ? 1 / tickC64ToC1541Ratio : 1;

    let tickCountC64 = 0;
    let tickCountC1541 = 0;

    let lastTickCountC64 = -1;
    let lastTickCountC1541 = -1;

    let c1541TickDone = false;

    let c1541EmulationHzNeeded = [];

    let motorAnimationIndex = 0;

    const wallClockStart = +new Date();

    const stopAfterSystemSeconds = 20;
    
    const consoleFramesPerSecond = 1;

    let worstNeededMHzCenter = 0;
    let worstNeededMHzRange = 0;

    let nextOutput = +new Date();
    let keepRunning = true;

    while (keepRunning) {
        const now = +new Date();

        if (now >= nextOutput) {
            nextOutput = now + (1000 / consoleFramesPerSecond);

            const c64Seconds = tickCountC64 / c64TicksPerSecond;
            const wallSeconds = (now - wallClockStart) / 1000;
            const speed = c64Seconds / wallSeconds;

            c64_print_screen();
            console.log(`C1541 ticks: ${Math.floor(tickCountC1541)}`);
            console.log(`System seconds: ${c64Seconds.toFixed(2)}  Wall seconds: ${wallSeconds.toFixed(0)}  Speed: ${speed.toFixed(3)}x`);

            const minNeededMHz = Math.ceil(Math.min.apply(Math, c1541EmulationHzNeeded) / 1e6);
            const maxNeededMHz = Math.ceil(Math.max.apply(Math, c1541EmulationHzNeeded) / 1e6);
            const sumNeeded = c1541EmulationHzNeeded.reduce((s, v) => s + v, 0);
            const avgNeededMHz = Math.ceil((sumNeeded / Math.max(1, c1541EmulationHzNeeded.length)) / 1e6);
            c1541EmulationHzNeeded.sort();
            const medianNeededMHz = (c1541EmulationHzNeeded.length > 0) ? c1541EmulationHzNeeded[Math.floor(c1541EmulationHzNeeded.length / 2)] / 1e6 : 0;

            const centerNeededMHz = (avgNeededMHz + medianNeededMHz) / 2;
            const rangeNeededMHz = centerNeededMHz - Math.min(avgNeededMHz, medianNeededMHz);
            if (c64Seconds > 0.05 && centerNeededMHz + rangeNeededMHz > worstNeededMHzCenter + worstNeededMHzRange) {
                worstNeededMHzCenter = centerNeededMHz;
                worstNeededMHzRange = rangeNeededMHz;
            }

            console.log(`C1541 Emulation MHz needed: ${centerNeededMHz.toFixed(1)} +/- ${rangeNeededMHz.toFixed(1)} => ${centerNeededMHz + rangeNeededMHz}`);
            console.log(`Worst Emulation MHz needed: ${worstNeededMHzCenter.toFixed(1)} +/- ${worstNeededMHzRange.toFixed(1)} => ${worstNeededMHzCenter + worstNeededMHzRange}`);
            const motorChar = (mcu.gpio[RP2_MOTOR_STATUS_PIN].value) ? displayMotorAnim[motorAnimationIndex] : " ";
            const ledChar = (mcu.gpio[RP2_LED_PIN].value) ? "*" : " ";
            console.log(`[${motorChar}] Motor  [${ledChar}] LED`);

            motorAnimationIndex = (motorAnimationIndex + 1) % displayMotorAnim.length;

            c1541EmulationHzNeeded.length = 0;
            keepRunning = stopAfterSystemSeconds <= 0 || c64Seconds < stopAfterSystemSeconds;
        }

        const mustTickC64 = Math.floor(tickCountC64) != Math.floor(lastTickCountC64);
        const mustTickC1541 = Math.floor(tickCountC1541) != Math.floor(lastTickCountC1541);

        if (mustTickC64) {
            tickC64();
        }

        if (mustTickC1541) {
            c1541TickDone = false;
            const startCycles = mcu.cycles;
            while (!c1541TickDone) {
                mcu.step();
            }
            const elapsed = mcu.cycles - startCycles;
            c1541EmulationHzNeeded.push(elapsed * c1541TicksPerSecond);
        }

        lastTickCountC64 = tickCountC64;
        tickCountC64 += c64TickDelta;

        lastTickCountC1541 = tickCountC1541;
        tickCountC1541 += c1541TickDelta;
    }
}

emu();

