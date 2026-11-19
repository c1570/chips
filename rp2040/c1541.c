//
// Created by crazydee on 26.12.25.
//

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "cycle_tracing.h"

// GPIO pin assignments
#define IEC_PIN_ATN      2
#define IEC_PIN_CLK      3
#define IEC_PIN_DATA     4
#define IEC_PIN_SRQ      29
// If using a 7406, those above are IN (inverted by 74LVC04 optionally)
#define IEC_PIN_RESET    5
// If using a 7406, those below are OUT (and get inverted)
#define IEC_PIN_CLK_OUT  6
#define IEC_PIN_DATA_OUT 7
#define IEC_PIN_SRQ_OUT  28

#define DISPLAY_RESET    26
#define DISPLAY_SDA      15
#define DISPLAY_SCL      14
#define DISPLAY_DC       27

#define DISK_CHANGE_PIN  8
#define MOTOR_STATUS_PIN 24
#define LED_PIN          25

#define CHIPS_IMPL
#define IS_RP2
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "../chips/chips_common.h"
#define likely(x) __builtin_expect(!!(x), 1)
#include "../chips/m6502_connomore64.h"
#include "../chips/m6522.h"
#include "../chips/clk.h"
#include "../chips/mem.h"
static bool drive_led_status = 0;
static bool drive_motor_status = 1;
static int drive_current_track = 0;
#define C1541_TRACK_CHANGED_HOOK(s,v) {cycle_info("track");drive_current_track=v;}
#define C1541_MOTOR_CHANGED_HOOK(s,v) gpio_put(MOTOR_STATUS_PIN,v)
#define C1541_LED_CHANGED_HOOK(s,v) {cycle_info("led  ");gpio_put(LED_PIN,v);}
#define C1541_CYCLE_INFO(a) cycle_info(a)
static volatile bool via2_gcr_dirty;   /* core0 wrote $1C00, core1 must re-prepare */
#define C1541_VIA2_GCR_DIRTY() (via2_gcr_dirty = true)
#include "iecbus_rp2.h"
#include "../systems/c1541.h"
#include "../tests/c1541-roms.h"

#include <time.h>

static struct {
    c1541_t c1541;
    bool keep_running;
} state;

static volatile uint32_t via2_start;
static volatile uint32_t via2_done;
static volatile bool via2_irq;

static void core1_entry(void) {
    cycle_info("bootc1   "); // core1 idle-while-core0-boots lands here (huge first sample)
    while (true) {
        /* speculate the GCR prepare while waiting for core0: it reads the
           committed state buffer and writes only the idle one, so core0's
           CPU tick cannot observe it. Inputs taken from sys (motor state,
           speed zone) are stable during the CPU tick except for $1C00
           writes, which set via2_gcr_dirty */
        uint32_t via2_in = _c1541_via2_gcr_prepare(&state.c1541);
        while (via2_start == 0) {
        }
        via2_start = 0;
        cycle_info("gcr-idle "); // waiting for core0 to hand over the VIA2 tick
        if (via2_gcr_dirty) {
            /* the speculation was wrong: re-prepare from the still-untouched
               committed buffer (with the final motor/speed state) */
            via2_gcr_dirty = false;
            via2_in = _c1541_via2_gcr_prepare(&state.c1541);
        }
        cycle_info("gcr  ");
        _c1541_via2_gcr_commit(&state.c1541);   /* flip the buffers */
        /* apply the byte-ready countdown (the only part that must run after
           the cpu tick: its s0/OV workaround decrements it directly) */
        if (via2_in & C1541_GCR_BYTE_READY) {
            state.c1541.byte_ready_countdown = 2;
        }
        if (state.c1541.byte_ready_countdown > 0) {
            state.c1541.byte_ready_countdown--;
        }
        via2_irq = (uint8_t)(_m6522_tick_io(&state.c1541.via_2, via2_in) >> 24);
        cycle_info("via2 ");
        via2_done = 1;
        _c1541_tick_via2_b(&state.c1541);
    }
}

void cleanup(int signal) {
    state.keep_running = false;
}

// Initialize GPIO pins for IEC bus open-collector operation
void init_iec_gpio(void) {
    // Build mask of all IEC pins
    const uint32_t iec_mask = (1u << IEC_PIN_DATA) |
                              (1u << IEC_PIN_CLK) |
                              (1u << IEC_PIN_ATN) |
                              (1u << IEC_PIN_SRQ) |
                              (1u << IEC_PIN_RESET);

    // Initialize all IEC pins
    gpio_init_mask(iec_mask);

    // IMPORTANT: Set all pins to output 0V first
    // This ensures that if a pin is set to output, it drives low (not high)
    gpio_put_masked(iec_mask, 0u);

    // Set all to input with pull-up initially (inactive state = high)
    gpio_set_dir_in_masked(iec_mask);
}

// Read incoming IEC signals from GPIOs
uint8_t read_iec_signals(void) {
    const uint32_t gpio = gpio_get_all();
    /* GPIO reads 1 when HIGH (inactive), 0 when LOW (active); IEC signal
       bits are active-low too, so an active line clears its signal bit */
    const uint32_t inactive =
          (((gpio >> IEC_PIN_ATN)   & 1u) << 2)    // IECLINE_ATN
        | (((gpio >> IEC_PIN_CLK)   & 1u) << 1)    // IECLINE_CLK
        | (((gpio >> IEC_PIN_DATA)  & 1u) << 0)    // IECLINE_DATA
        | (((gpio >> IEC_PIN_SRQ)   & 1u) << 3)    // IECLINE_SRQIN
        | (((gpio >> IEC_PIN_RESET) & 1u) << 4);   // IECLINE_RESET
    return 0xE0u | (uint8_t)inactive;
}

// Write outgoing IEC signals using open-collector logic
void write_iec_signals(uint8_t signals) {
    // Open-collector output: GPIO_OUT (dir=1) pulls low (active),
    // GPIO_IN (dir=0) releases the line (inactive/high via pull-up).
    const uint32_t mask = (1u << IEC_PIN_DATA) | (1u << IEC_PIN_CLK) |
                          (1u << IEC_PIN_ATN) | (1u << IEC_PIN_RESET) |
                          (1u << IEC_PIN_SRQ);
    const uint32_t inv = (uint32_t)(uint8_t)~signals;
    uint32_t dirs = 0;
    dirs |= ((inv >> 0) & 1u) << IEC_PIN_DATA;   // IECLINE_DATA
    dirs |= ((inv >> 1) & 1u) << IEC_PIN_CLK;    // IECLINE_CLK
    dirs |= ((inv >> 2) & 1u) << IEC_PIN_ATN;    // IECLINE_ATN
    dirs |= ((inv >> 4) & 1u) << IEC_PIN_RESET;  // IECLINE_RESET
    // SRQ always input for now (dir bit left 0)
    gpio_set_dir_masked(mask, dirs);
}

#include "../docs/1541_test_demo.h"
#pragma message "Using ../docs/1541_test_demo.h as disk image"
bool c1541_fetch_track(c1541_t* sys) {
    const uint8_t* gcr_data = (const uint8_t*) gcr_1541_test_demo_g64;
    const uint32_t half_track_data_offset = ((uint32_t *)(gcr_data + 0xc))[sys->half_track];
    if(!half_track_data_offset) return false;
    const uint32_t data_size = ((uint32_t)gcr_data[half_track_data_offset+0]) + ((uint32_t)gcr_data[half_track_data_offset+1] << 8);
    for (uint16_t i=0; i < data_size; i++) {
        sys->gcr_bytes[i] = gcr_data[half_track_data_offset+2+i];
    }
    sys->gcr_bytes[data_size] = 0; // mark track done, protection against memory garbage
    sys->gcr_size = data_size;
    /* the GCR read cursor must be reset whenever the track data changes */
    c1541_gcr_state_t* const gcr = &sys->gcr_state[sys->gcr_committed];
    gcr->gcr_byte_pos = 0;
    gcr->gcr_bit_mask = 0x80;
    gcr->gcr_current_byte = sys->gcr_bytes[0];
    return true;
}

int main(int argc, char **argv) {
    struct timespec ts;
    struct timespec sleep_ts;
    long next_tick_ns = 0;
    long current_ns = 0;
    long last_ns = 0;
    long start_ns = 0;
    long run_ticks = 0;
    long sleep_ns = 0;
    long total_ns = 0;
    c1541_desc_t floppy_desc;
    const long ticks_per_sec = 1000000;
    const long ns_per_sec = 1000000000;
    const long ns_per_tick = ns_per_sec / ticks_per_sec;
    uint8_t last_device_signals;
    uint8_t device_signals;
    long last_second_ns = 0;
    long current_ticks_per_second;

    sleep_ts.tv_sec = 0;

    state.keep_running = true;

    floppy_desc.roms.c000_dfff.ptr = dump_1541_c000_325302_01_bin;
    floppy_desc.roms.c000_dfff.size = 8192;
    floppy_desc.roms.e000_ffff.ptr = dump_1541_e000_901229_06aa_bin;
    floppy_desc.roms.e000_ffff.size = 8192;
    floppy_desc.iec_bus = NULL;

    uint tick = 0;
    uint ftick = 0;
    uint out_signals = 0;
    // Initialize RP2040 GPIOs for IEC bus (open collector logic)
    init_iec_gpio();
    gpio_init(MOTOR_STATUS_PIN);
    gpio_set_dir(MOTOR_STATUS_PIN, 1); // output
    gpio_put(MOTOR_STATUS_PIN, drive_motor_status);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, 1); // output
    gpio_put(LED_PIN, drive_led_status);

    c1541_init(&state.c1541, &floppy_desc);
    iecbus_device_t* host_iec = iec_connect(&state.c1541.iec_bus, false);
    c1541_fetch_track(&state.c1541); // read initial track
    multicore_launch_core1(core1_entry);
    cycle_info("bootc0   "); // everything before this was one-time boot work

    do {
      // Read IEC incoming signals from GPIOs and update the IEC bus
      iec_set_from_host_signals(read_iec_signals());
      cycle_info("gpioin   ");

      uint32_t via1_in;
      uint64_t pins = _c1541_tick_core0a(&state.c1541, state.c1541.pins, &via1_in);
      via2_done = 0;
      via2_start = 1;
      pins = _c1541_tick_core0b(&state.c1541, pins, via1_in);
      cycle_info("c1541");
      //if((tick&0xfffff)==0) printf("%d %d %04x\n", tick, state.c1541.iec_bus->master_tick, state.c1541.cpu.PC);
      tick++;

      const uint out_new_signals = iec_get_drive_out_signals();
      if (out_new_signals != out_signals) {
        out_signals = out_new_signals;
        write_iec_signals(out_new_signals);
      }
      cycle_info("tick-waitc1  "); // IEC output update done, start waiting for core1
      while (via2_done == 0) {
      }
      if (via2_irq) {
        pins |= M6502_IRQ;
      }
      state.c1541.pins = pins;
      //_c1541_debug_out_processor_pc(tick, &state.c1541, state.c1541.pins, state.c1541.via_1.pins);
      cycle_info("*tick"); // self-referential: the runner measures these against the PREVIOUS *tick (full tick wall time) and ticks the c64 emulation
    } while (state.keep_running);

    c1541_discard(&state.c1541);

    return 0;
}
