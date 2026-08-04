/*
 * RP2 C1541 Test Runner
 *
 * Runs the RP2 C1541 firmware in rp2350js-c and interfaces with the C64 emulator.
 * cts2c-transpiled C build of rp2350js.
 */

#include "rp2350js-c.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── C64 emulator (c64_emulation_wrapper.c, linked separately) ──────────────── */
extern void c64_emulation_init(void);
extern void c64_emulation_tick(void);
extern void c64_set_iec_gpio(uint8_t state);
extern uint8_t c64_get_iec_bus(void);
extern void c64_print_screen(void);
extern void c64_load_cartridge(const char* filename);
extern uint8_t c64_ram_read(uint16_t addr);

#define FIRMWARE_PATH "../rp2040/build/c1541.uf2"

/* IEC GPIO pins on RP2 */
#define IEC_GPIO_ATN 2
#define IEC_GPIO_CLK 3
#define IEC_GPIO_DATA 4
#define IEC_GPIO_RESET 5

#define RP2_MOTOR_STATUS_PIN 24
#define RP2_LED_PIN 25

/* IEC line definitions (must match iecbus.h) */
#define IECLINE_DATA (1 << 0)
#define IECLINE_CLK (1 << 1)
#define IECLINE_ATN (1 << 2)
#define IECLINE_RESET (1 << 4)

/* GPIOPinState.Low, i.e. the pin is actively driven low (see gpio-pin.ts). */
#define GPIO_PIN_STATE_LOW 0

/* PAL. The NTSC figures from the JS runner (1022727 / 30000.0/1001) are unused. */
#define C64_TICKS_PER_SECOND 985249.0
#define C64_FRAMES_PER_SECOND 25.0
#define C1541_TICKS_PER_SECOND 1000000.0

#define STOP_AFTER_SYSTEM_SECONDS 20.0
#define CONSOLE_FRAMES_PER_SECOND 1.0

static RP2350* mcu;

/* Set by the onTrace callback when the firmware reaches its "tick " marker; see
 * cycle_info() in the firmware's cycle_tracing.h. This is the runner's clock. */
static bool c1541_tick_done = false;

/* ── UART ───────────────────────────────────────────────────────────────────── */
static void on_uart_byte(void* ctx, int32_t value) {
  (void)ctx;
  putchar((int)(value & 0xff));
}

/* ── Trace markers ──────────────────────────────────────────────────────────── */
static void on_trace(void* ctx, int32_t core_number, int32_t pc, const char* tag) {
  (void)ctx;
  (void)core_number;
  if (tag != NULL && strcmp(tag, "tick ") == 0) {
    c1541_tick_done = true;
  } else {
    printf("%" PRId64 " PC 0x%x tag %s\n", (int64_t)RP2350_cycles_get(mcu), (unsigned)pc,
           tag ? tag : "");
  }
}

/* ── Milliseconds since an arbitrary epoch, matching the JS `+new Date()` use ── */
static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* ── Per-tick "MHz needed" samples, cleared once per console frame ──────────── */
static double* hz_needed = NULL;
static size_t hz_needed_len = 0;
static size_t hz_needed_cap = 0;

static void hz_needed_push(double v) {
  if (hz_needed_len == hz_needed_cap) {
    size_t cap = hz_needed_cap ? hz_needed_cap * 2 : 1u << 20;
    double* grown = realloc(hz_needed, cap * sizeof(*grown));
    if (grown == NULL) {
      fprintf(stderr, "out of memory growing the MHz-needed sample buffer\n");
      exit(1);
    }
    hz_needed = grown;
    hz_needed_cap = cap;
  }
  hz_needed[hz_needed_len++] = v;
}

static int cmp_double(const void* a, const void* b) {
  double x = *(const double*)a, y = *(const double*)b;
  return (x > y) - (x < y);
}

/* ── C64 <-> RP2 IEC bus glue ───────────────────────────────────────────────── */
static uint8_t last_iec_state = 0xff;

static void tick_c64(void) {
  /* Read RP2 IEC GPIO outputs.
   * GPIO value: Low=active (0), High=inactive (1)
   * IEC format uses: 0=active, 1=inactive - same as GPIO direction logic! */
  bool data_out = GPIOPin_value_get(mcu->gpio[IEC_GPIO_DATA]) != GPIO_PIN_STATE_LOW;
  bool clk_out = GPIOPin_value_get(mcu->gpio[IEC_GPIO_CLK]) != GPIO_PIN_STATE_LOW;
  bool atn_out = GPIOPin_value_get(mcu->gpio[IEC_GPIO_ATN]) != GPIO_PIN_STATE_LOW;
  bool reset_out = GPIOPin_value_get(mcu->gpio[IEC_GPIO_RESET]) != GPIO_PIN_STATE_LOW;

  /* Build IEC state byte (format matches IECLINE_* bits: 0=DATA, 1=CLK, 2=ATN, 4=RESET) */
  uint8_t iec_state = 0xff; /* All inactive (high) by default */
  if (!data_out) iec_state &= (uint8_t)~IECLINE_DATA;
  if (!clk_out) iec_state &= (uint8_t)~IECLINE_CLK;
  if (!atn_out) iec_state &= (uint8_t)~IECLINE_ATN;
  if (!reset_out) iec_state &= (uint8_t)~IECLINE_RESET;

  /* Send the RP2's IEC signals to the C64 */
  if (iec_state != last_iec_state) {
    c64_set_iec_gpio(iec_state);
    last_iec_state = iec_state;
  }

  c64_emulation_tick();

  /* Get combined IEC bus state (C64 + RP2) and apply it to the RP2's GPIO inputs */
  uint8_t bus_state = c64_get_iec_bus();
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_DATA], (bus_state & IECLINE_DATA) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_CLK], (bus_state & IECLINE_CLK) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_ATN], (bus_state & IECLINE_ATN) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_RESET], (bus_state & IECLINE_RESET) != 0);
}

int main(void) {
  static const char* const motor_anim[] = {"b", "d", "q", "p"};
  const size_t motor_anim_len = sizeof(motor_anim) / sizeof(motor_anim[0]);

  printf("Initializing C64 emulator...\n");
  c64_emulation_init();

#ifdef AUTOTEST
  printf("AUTOTEST: loading cartridge...\n");
  c64_load_cartridge("cart-br-read.bin");
#endif

  printf("Initializing RP2...\n");
  RP2350Options options = {.coreArch = "riscv", .loadFirmware = NULL};
  mcu = RP2350_new(&options);
  RP2350_loadFirmware(mcu, FIRMWARE_PATH, NULL);

  mcu->uart[0]->onByte_fn = on_uart_byte;
  mcu->onTrace_fn = on_trace;

  /* The C64 runs slower than the C1541 (0.985 MHz vs 1 MHz), so the C1541 advances every
   * iteration and the C64 advances whenever its fractional tick counter crosses an
   * integer. Kept as floating point deltas exactly as in the JS runner. */
  const double tick_c64_to_c1541_ratio = C64_TICKS_PER_SECOND / C1541_TICKS_PER_SECOND;
  const double c64_tick_delta = (tick_c64_to_c1541_ratio > 1.0) ? 1.0 : tick_c64_to_c1541_ratio;
  const double c1541_tick_delta =
      (tick_c64_to_c1541_ratio > 1.0) ? 1.0 / tick_c64_to_c1541_ratio : 1.0;

  double tick_count_c64 = 0.0;
  double tick_count_c1541 = 0.0;
  double last_tick_count_c64 = -1.0;
  double last_tick_count_c1541 = -1.0;

  size_t motor_animation_index = 0;

  const double wall_clock_start = now_ms();
  double next_output = wall_clock_start;

  double worst_needed_mhz_center = 0.0;
  double worst_needed_mhz_range = 0.0;

  bool keep_running = true;
  while (keep_running) {
    const double now = now_ms();

    if (now >= next_output) {
      next_output = now + (1000.0 / CONSOLE_FRAMES_PER_SECOND);

      const double c64_seconds = tick_count_c64 / C64_TICKS_PER_SECOND;
      const double wall_seconds = (now - wall_clock_start) / 1000.0;
      const double speed = c64_seconds / wall_seconds;

      c64_print_screen();
      printf("C1541 ticks: %.0f\n", floor(tick_count_c1541));
      printf("System seconds: %.2f  Wall seconds: %.0f  Speed: %.3fx\n", c64_seconds, wall_seconds,
             speed);

      double sum_needed = 0.0;
      for (size_t i = 0; i < hz_needed_len; i++) sum_needed += hz_needed[i];
      const double avg_needed_mhz =
          ceil((sum_needed / (double)(hz_needed_len > 0 ? hz_needed_len : 1)) / 1.0e6);
      /* Numeric sort. The JS runner called `.sort()` with no comparator, which sorts
       * numbers lexicographically ("1e8" before "9e7"), so its median was wrong; this
       * reports the real one and can differ from the JS output for that reason. */
      qsort(hz_needed, hz_needed_len, sizeof(*hz_needed), cmp_double);
      const double median_needed_mhz =
          (hz_needed_len > 0) ? hz_needed[hz_needed_len / 2] / 1.0e6 : 0.0;

      const double center_needed_mhz = (avg_needed_mhz + median_needed_mhz) / 2.0;
      const double range_needed_mhz =
          center_needed_mhz - fmin(avg_needed_mhz, median_needed_mhz);
      if (c64_seconds > 0.05 &&
          center_needed_mhz + range_needed_mhz > worst_needed_mhz_center + worst_needed_mhz_range) {
        worst_needed_mhz_center = center_needed_mhz;
        worst_needed_mhz_range = range_needed_mhz;
      }

      printf("C1541 Emulation MHz needed: %.1f +/- %.1f => %g\n", center_needed_mhz,
             range_needed_mhz, center_needed_mhz + range_needed_mhz);
      printf("Worst Emulation MHz needed: %.1f +/- %.1f => %g\n", worst_needed_mhz_center,
             worst_needed_mhz_range, worst_needed_mhz_center + worst_needed_mhz_range);

      const char* motor_char =
          GPIOPin_value_get(mcu->gpio[RP2_MOTOR_STATUS_PIN]) ? motor_anim[motor_animation_index] : " ";
      const char* led_char = GPIOPin_value_get(mcu->gpio[RP2_LED_PIN]) ? "*" : " ";
      printf("[%s] Motor  [%s] LED\n", motor_char, led_char);
      fflush(stdout);

      motor_animation_index = (motor_animation_index + 1) % motor_anim_len;

      hz_needed_len = 0;
      keep_running = STOP_AFTER_SYSTEM_SECONDS <= 0.0 || c64_seconds < STOP_AFTER_SYSTEM_SECONDS;
    }

    const bool must_tick_c64 = floor(tick_count_c64) != floor(last_tick_count_c64);
    const bool must_tick_c1541 = floor(tick_count_c1541) != floor(last_tick_count_c1541);

    if (must_tick_c64) {
      tick_c64();
    }

#ifdef AUTOTEST
    /* Check cartridge completion marker at $C100 */
    if (must_tick_c64) {
      uint8_t prog = c64_ram_read(0xC100);
      bool timeout = tick_count_c64 > 2300000;
      if (prog == 0xFF || prog == 0xEE || timeout) {
        static const uint8_t expected[18] = {
            0x01, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00};
        int pass = (prog == 0xFF);
        if (pass) {
          for (int i = 0; i < 18; i++) {
            if (c64_ram_read(0xC000 + i) != expected[i]) {
              pass = 0;
              break;
            }
          }
        }
        double secs = tick_count_c64 / C64_TICKS_PER_SECOND;
        printf("AUTOTEST: %s (PROG=$%02X, %.2fs C64, %.0f C1541 ticks)\n",
               pass ? "PASS" : "FAIL", prog, secs, tick_count_c1541);
        if (!pass) {
          printf("  Buffer $C000:");
          for (int i = 0; i < 18; i++) printf(" %02X", c64_ram_read(0xC000 + i));
          printf("\n  Expected:    ");
          for (int i = 0; i < 18; i++) printf(" %02X", expected[i]);
          printf("\n");
        }
        free(hz_needed);
        return pass ? 0 : 1;
      }
    }
#endif

    if (must_tick_c1541) {
      c1541_tick_done = false;
      const int64_t start_cycles = RP2350_cycles_get(mcu);
      while (!c1541_tick_done) {
        RP2350_step(mcu);
      }
      const int64_t elapsed = RP2350_cycles_get(mcu) - start_cycles;
      hz_needed_push((double)elapsed * C1541_TICKS_PER_SECOND);
    }

    last_tick_count_c64 = tick_count_c64;
    tick_count_c64 += c64_tick_delta;

    last_tick_count_c1541 = tick_count_c1541;
    tick_count_c1541 += c1541_tick_delta;
  }

  free(hz_needed);
  return 0;
}
