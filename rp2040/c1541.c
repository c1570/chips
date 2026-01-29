//
// Created by crazydee on 26.12.25.
//

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "cycle_tracing.h"

// GPIO pin assignments
#define IEC_PIN_DATA    2   // GPIO for DATA line
#define IEC_PIN_CLK     3   // GPIO for CLK line
#define IEC_PIN_ATN     4   // GPIO for ATN line
#define IEC_PIN_SRQ     5   // GPIO for SRQIN line (input only, reserved for future)
#define IEC_PIN_RESET   6   // GPIO for RESET line
#define MOTOR_STATUS_PIN 8
#define LED_PIN 25

#define CHIPS_IMPL
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
#define C1541_LED_CHANGED_HOOK(s,v) {cycle_info("led");gpio_put(LED_PIN,v);}
#include "iecbus_rp2.h"
#include "../systems/c1541.h"
#include "../tests/c1541-roms.h"

#include <time.h>

static struct {
    c1541_t c1541;
    bool keep_running;
} state;

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
    uint8_t signals = 0xFF;  // Start with all inactive (high)

    // GPIO reads return 1 when HIGH (inactive), 0 when LOW (active)
    if (!gpio_get(IEC_PIN_ATN))   signals &= ~IECLINE_ATN;
    if (!gpio_get(IEC_PIN_DATA))  signals &= ~IECLINE_DATA;
    if (!gpio_get(IEC_PIN_CLK))   signals &= ~IECLINE_CLK;
    if (!gpio_get(IEC_PIN_RESET)) signals &= ~IECLINE_RESET;
    if (!gpio_get(IEC_PIN_SRQ))   signals &= ~IECLINE_SRQIN;

    return signals;
}

// Write outgoing IEC signals using open-collector logic
void write_iec_signals(uint8_t signals) {
    // Open-collector output: set direction to output to pull low (active),
    // or input to release (inactive/high via pull-up)

    // DATA line: pull low if active, otherwise release (input)
    if (signals & IECLINE_DATA) {
        gpio_set_dir(IEC_PIN_DATA, GPIO_IN);  // Release (inactive)
    } else {
        gpio_set_dir(IEC_PIN_DATA, GPIO_OUT); // Pull low (active)
    }

    // CLK line
    if (signals & IECLINE_CLK) {
        gpio_set_dir(IEC_PIN_CLK, GPIO_IN);   // Release (inactive)
    } else {
        gpio_set_dir(IEC_PIN_CLK, GPIO_OUT);  // Pull low (active)
    }

    // ATN line (typically input-only for drive, but handle for completeness)
    if (signals & IECLINE_ATN) {
        gpio_set_dir(IEC_PIN_ATN, GPIO_IN);   // Release (inactive)
    } else {
        gpio_set_dir(IEC_PIN_ATN, GPIO_OUT);  // Pull low (active)
    }

    // RESET line
    if (signals & IECLINE_RESET) {
        gpio_set_dir(IEC_PIN_RESET, GPIO_IN); // Release (inactive)
    } else {
        gpio_set_dir(IEC_PIN_RESET, GPIO_OUT);// Pull low (active)
    }

    // always input for now
    gpio_set_dir(IEC_PIN_SRQ, GPIO_IN);
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
    uint in_signals = 0;
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

    do {
      // Read IEC incoming signals from GPIOs and update the IEC bus
      iec_set_from_host_signals(read_iec_signals());
      c1541_tick(&state.c1541);
      //if((tick&0xfffff)==0) printf("%d %d %04x\n", tick, state.c1541.iec_bus->master_tick, state.c1541.cpu.PC);
      tick++;

      const uint in_new_signals = iec_get_signals(state.c1541.iec_bus);
      const uint out_new_signals = iec_get_drive_out_signals();
      if((out_new_signals != out_signals) || (in_new_signals != in_signals)) {
        out_signals = out_new_signals;
        in_signals = in_new_signals;
        //printf("1541 iec output - DATA(%d) CLK(%d), input - ATN(%d) DATA(%d) CLK(%d)\n", IEC_DATA_ACTIVE(out_signals), IEC_CLK_ACTIVE(out_signals), IEC_ATN_ACTIVE(in_signals), IEC_DATA_ACTIVE(in_signals), IEC_CLK_ACTIVE(in_signals));
        write_iec_signals(out_new_signals);
      }
      //_c1541_debug_out_processor_pc(tick, &state.c1541, state.c1541.pins, state.c1541.via_1.pins);
      cycle_info("tick "); // rp2040js catches these and ticks the c64 emulation
    } while (state.keep_running);

    c1541_discard(&state.c1541);

    return 0;
}
