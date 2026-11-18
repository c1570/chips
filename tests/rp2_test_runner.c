/*
 * RP2 C1541 Test Runner
 *
 * Runs the RP2 C1541 firmware in rp2350js-c and interfaces with the C64 emulator.
 * cts2c-transpiled C build of rp2350js.
 */

#include "rp2350js-c.h"
#include "sdcard_model.h"

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
extern uint8_t c64_get_text_screen(uint8_t* chars_out, uint8_t* colors_out);
extern uint16_t c64_get_pc(void);

#ifndef FIRMWARE_PATH
#define FIRMWARE_PATH "../../sd2iec/obj-rp2350-rppico-direct/sd2iec.uf2"
#endif

/* IEC GPIO pins on RP2 */
#define IEC_GPIO_ATN 2
#define IEC_GPIO_CLK 3
#define IEC_GPIO_DATA 4
#define IEC_GPIO_RESET 5

/* SD card on SPI0 (see sd2iec src/rppico/arch-config.h) */
#define SD_SPI_NUM 0
#define SD_GPIO_CS 17
#define SD_GPIO_DETECT 22

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
#define RP2_CYCLES_PER_SECOND 150000000.0

/* One C1541 tick is 1us, i.e. 150 cycles of the 150MHz RP2 */
#define RP2_CYCLES_PER_C1541_TICK ((int64_t)(RP2_CYCLES_PER_SECOND / C1541_TICKS_PER_SECOND))

#define STOP_AFTER_SYSTEM_SECONDS 20.0
#define CONSOLE_FRAMES_PER_SECOND 1.0

static RP2350* mcu;
static sdcard_t* sd_card;

/* ── UART ───────────────────────────────────────────────────────────────────── */
static void on_uart_byte(void* ctx, int32_t value) {
  (void)ctx;
  putchar((int)(value & 0xff));
}

/* ── Trace markers ──────────────────────────────────────────────────────────── */
static void on_trace(void* ctx, int32_t core_number, int32_t pc, const char* tag) {
  (void)ctx;
  (void)core_number;
  printf("%" PRId64 " PC 0x%x tag %s\n", (int64_t)RP2350_cycles_get(mcu), (unsigned)pc,
         tag ? tag : "");
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
/* IEC_TRACE=1: print every IEC line change (set in main, time kept by the main loop) */
static bool iec_trace;
static double iec_trace_time_us;

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
  if (iec_trace) {
    static uint8_t last_bus = 0xff, last_dev = 0xff;
    if (bus_state != last_bus || iec_state != last_dev) {
      printf("IEC %10.1fus  bus ATN=%d CLK=%d DATA=%d  sd2iec drives CLK=%d DATA=%d  C64 PC $%04x  RP2 PC 0x%08x\n",
             iec_trace_time_us, !!(bus_state & IECLINE_ATN), !!(bus_state & IECLINE_CLK),
             !!(bus_state & IECLINE_DATA), !(iec_state & IECLINE_CLK), !(iec_state & IECLINE_DATA),
             c64_get_pc(), (unsigned)CortexM33Core_PC_get(mcu->core[0].obj));
      last_bus = bus_state;
      last_dev = iec_state;
    }
  }
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_DATA], (bus_state & IECLINE_DATA) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_CLK], (bus_state & IECLINE_CLK) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_ATN], (bus_state & IECLINE_ATN) != 0);
  GPIOPin_setInputValue(mcu->gpio[IEC_GPIO_RESET], (bus_state & IECLINE_RESET) != 0);
}

/* ── SD card ────────────────────────────────────────────────────────────────── */
static int env_int(const char* name, int fallback) {
  const char* v = getenv(name);
  return (v && *v) ? atoi(v) : fallback;
}

/* SD2IEC_SDIMG selects the card image; SD_* variables tune the card model. */
static void open_sd_card(void) {
  const char* image = getenv("SD2IEC_SDIMG");
  if (image == NULL || *image == '\0') return;

  sdcard_opts_t opts = sdcard_default_opts();
  opts.sdsc = env_int("SD_SDSC", 0) != 0;
  opts.ncr = env_int("SD_NCR", opts.ncr);
  opts.nac = env_int("SD_NAC", opts.nac);
  opts.acmd41_busy_polls = env_int("SD_ACMD41_POLLS", opts.acmd41_busy_polls);
  opts.write_busy = env_int("SD_WRITE_BUSY", opts.write_busy);
  opts.trace = env_int("SD_TRACE", 0);
  opts.fault_read_crc_nth = env_int("SD_FAULT_READ_CRC", 0);
  opts.fault_cmd_crc_nth = env_int("SD_FAULT_CMD_CRC", 0);

  sd_card = sdcard_open(image, &opts);
  if (sd_card == NULL) {
    fprintf(stderr, "cannot open SD card image %s\n", image);
    exit(1);
  }
  printf("SD card: %s (%u sectors)\n", image, sdcard_sector_count(sd_card));
  if (opts.trace) setvbuf(stdout, NULL, _IOLBF, 0); /* keep UART and SD trace in order */
}

/* Called for every byte the SPI peripheral shifts out. CS is a plain GPIO. */
static void on_spi_transmit(void* ctx, int32_t value) {
  RPSPI* spi = ctx;
  const bool cs_active = GPIOPin_value_get(mcu->gpio[SD_GPIO_CS]) == GPIO_PIN_STATE_LOW;
  RPSPI_completeTransmit(spi, sdcard_xfer(sd_card, cs_active, (uint8_t)value));
}

/* Model the rppico-direct board's external pull-ups. The simulator defaults
 * raw GPIO inputs to low, which would make the address jumpers look closed
 * (device address 11 instead of 8). */
static void init_board_inputs(void) {
  GPIOPin_setInputValue(mcu->gpio[10], true); /* BUTTON_NEXT */
  GPIOPin_setInputValue(mcu->gpio[11], true); /* BUTTON_PREV */
  GPIOPin_setInputValue(mcu->gpio[20], true); /* ADDR_PIN_A */
  GPIOPin_setInputValue(mcu->gpio[21], true); /* ADDR_PIN_B */
  /* SD detect switch closes to GND when a card is inserted */
  GPIOPin_setInputValue(mcu->gpio[SD_GPIO_DETECT], sd_card == NULL);
}

#ifdef DIRTEST
/* ── Directory listing check ────────────────────────────────────────────────── */
/* After LOAD"$",8 the directory is a BASIC program at $0801: the line number is
 * the block count and the text holds the quoted name and type. The check reads it
 * from RAM, so long listings that scroll off the screen are covered too. Entries
 * are compared in the canonical form <blocks> "<name>" <type>; the header line is
 * HEADER "<name>" <id>. Bytes outside 0x20..0x7e are written as \xNN. */

/* FAT root of the make_sd_image.sh card (no CD). The header and the free blocks
 * depend on the image: DIRTEST_FAT=16 or 32 (default) selects them. */
static const char* const DIR_EXPECT_ROOT[] = {
    NULL, /* header, see root_expectation() */
    "1 \"HELLO.PRG\" PRG",
    "40 \"BIG.PRG\" PRG",
    "3 \"README\" PRG",
    "3 \"LONGMI\\xff1.PRG\" PRG", /* sd2iec shows the alias '~' as PETSCII pi */
    "689 \"1541_T\\xff1.D64\" PRG",
    "689 \"IMAGE.D64\" PRG",
    "0 \"GAMES\" DIR",
    NULL, /* blocks free */
};

/* Header ID and free blocks of the two make_sd_image.sh layouts */
static void root_expectation(char* header, size_t header_cap, char* free_line, size_t free_cap) {
  const int fat = env_int("DIRTEST_FAT", 32);
  snprintf(header, header_cap, "HEADER \"SD2IECTEST      \" %d 2A", fat);
  /* sd2iec caps the count at 65535; the FAT16 image has 16 MiB */
  snprintf(free_line, free_cap, "%s BLOCKS FREE.", fat == 16 ? "7985" : "65535");
}

/* chips/docs/1541_test_demo.d64 (SD2IEC_CD=IMAGE.D64) */
static const char* const DIR_EXPECT_D64[] = {
    "HEADER \"TEST/DEMO  1/85 \" 84 2A",
    "14 \"HOW TO USE\" PRG",
    "8 \"HOW PART TWO\" PRG",
    "7 \"HOW PART THREE\" PRG",
    "4 \"VIC-20 WEDGE\" PRG",
    "1 \"C-64 WEDGE\" PRG",
    "4 \"DOS 5.1\" PRG",
    "9 \"PRINTER TEST\" PRG",
    "6 \"DISK ADDR CHANGE\" PRG",
    "12 \"VIEW BAM\" PRG",
    "15 \"DISPLAY T&S\" PRG",
    "4 \"CHECK DISK\" PRG",
    "11 \"PERFORMANCE TEST\" PRG",
    "5 \"SEQ.FILE.DEMO\" PRG",
    "18 \"REL.FILE.DEMO\" PRG",
    "7 \"SD.BACKUP.C16\" PRG",
    "7 \"SD.BACKUP.PLUS4\" PRG",
    "10 \"SD.BACKUP.C64\" PRG",
    "7 \"PRINT.64.UTIL\" PRG",
    "7 \"PRINT.C16.UTIL\" PRG",
    "7 \"PRINT.+4.UTIL\" PRG",
    "13 \"UNI-COPY\" PRG",
    "30 \"C64 BASIC DEMO\" PRG",
    "35 \"+4 BASIC DEMO\" PRG",
    "8 \"LOAD ADDRESS\" PRG",
    "7 \"UNSCRATCH\" PRG",
    "5 \"HEADER CHANGE\" PRG",
    "319 BLOCKS FREE.",
};

#define DIRTEST_TIMEOUT_SECONDS 19.0
#define DIR_MAX_LINES 160
#define BASIC_START 0x0801

static void screen_lines(char lines[25][41]) {
  uint8_t chars[1000], colors[1000];
  c64_get_text_screen(chars, colors);
  for (int y = 0; y < 25; y++) {
    memcpy(lines[y], &chars[y * 40], 40);
    int len = 40;
    while (len > 0 && lines[y][len - 1] == ' ') len--;
    lines[y][len] = '\0';
  }
}

static uint16_t ram_word(uint16_t addr) {
  return (uint16_t)(c64_ram_read(addr) | c64_ram_read((uint16_t)(addr + 1)) << 8);
}

/* Append PETSCII bytes [from, to) to @out, escaping non-printable bytes */
static void append_text(char* out, size_t cap, const uint8_t* from, const uint8_t* to) {
  size_t len = strlen(out);
  for (const uint8_t* p = from; p < to && len + 5 < cap; p++) {
    if (*p >= 0x20 && *p <= 0x7e)
      out[len++] = (char)*p;
    else
      len += (size_t)snprintf(out + len, cap - len, "\\x%02x", *p);
  }
  out[len] = '\0';
}

/* Canonical form of one directory line, see above */
static void canonical_dir_line(uint16_t blocks, const uint8_t* text, size_t len, bool first, char* out,
                               size_t cap) {
  out[0] = '\0';
  const uint8_t* end = text + len;
  const uint8_t* q1 = memchr(text, '"', len);
  const uint8_t* q2 = q1 ? memchr(q1 + 1, '"', (size_t)(end - q1 - 1)) : NULL;
  if (q1 == NULL || q2 == NULL) {
    /* "BLOCKS FREE." and other lines without a name */
    while (text < end && *text == ' ') text++;
    while (end > text && end[-1] == ' ') end--;
    snprintf(out, cap, "%u ", blocks);
    append_text(out, cap, text, end);
    return;
  }
  const uint8_t* rest = q2 + 1;
  while (rest < end && *rest == ' ') rest++;
  while (end > rest && end[-1] == ' ') end--;
  if (first)
    snprintf(out, cap, "HEADER \"");
  else
    snprintf(out, cap, "%u \"", blocks);
  append_text(out, cap, q1 + 1, q2);
  strncat(out, "\" ", cap - strlen(out) - 1);
  append_text(out, cap, rest, end);
}

/* Read the loaded directory program; returns the number of lines */
static int read_dir_program(char lines[][64], int max_lines) {
  uint16_t ptr = BASIC_START;
  int n = 0;
  while (n < max_lines) {
    const uint16_t next = ram_word(ptr);
    if (next == 0) break;
    const uint16_t number = ram_word((uint16_t)(ptr + 2));
    uint8_t text[256];
    size_t len = 0;
    for (uint16_t a = (uint16_t)(ptr + 4); len < sizeof(text) && c64_ram_read(a) != 0; a++)
      text[len++] = c64_ram_read(a);
    canonical_dir_line(number, text, len, n == 0, lines[n], sizeof(lines[n]));
    n++;
    if (next <= ptr) break; /* corrupt link */
    ptr = next;
  }
  return n;
}

/* Returns 1 for pass, 0 for fail, -1 while LIST has not finished yet. */
static int check_dir_listing(bool timed_out) {
  /* LIST is done once "BLOCKS FREE." is followed by READY. on screen */
  char screen[25][41];
  screen_lines(screen);
  bool listed = false;
  for (int y = 0; y + 1 < 25 && !listed; y++)
    listed = strstr(screen[y], "BLOCKS FREE.") != NULL && strcmp(screen[y + 1], "READY.") == 0;
  if (!listed && !timed_out) return -1;

  /* Expect the D64 after a CD unless DIRTEST_EXPECT=root (CD that must fail) */
  const char* cd = getenv("SD2IEC_CD");
  const char* expect_mode = getenv("DIRTEST_EXPECT");
  const bool in_image = expect_mode != NULL ? strcmp(expect_mode, "d64") == 0 : (cd != NULL && *cd != '\0');
  const char* expect[DIR_MAX_LINES];
  int n_expect = 0;
  static char root_header[64], root_free[32];
  if (in_image) {
    for (size_t i = 0; i < sizeof(DIR_EXPECT_D64) / sizeof(DIR_EXPECT_D64[0]); i++)
      expect[n_expect++] = DIR_EXPECT_D64[i];
  } else {
    root_expectation(root_header, sizeof(root_header), root_free, sizeof(root_free));
    const int n_root = (int)(sizeof(DIR_EXPECT_ROOT) / sizeof(DIR_EXPECT_ROOT[0]));
    for (int i = 0; i < n_root; i++)
      expect[n_expect++] = i == 0 ? root_header : i == n_root - 1 ? root_free : DIR_EXPECT_ROOT[i];
  }

  static char got[DIR_MAX_LINES][64];
  const int n_got = read_dir_program(got, DIR_MAX_LINES);

  int ok = listed && n_got == n_expect;
  for (int i = 0; ok && i < n_expect; i++) ok = strcmp(got[i], expect[i]) == 0;

  if (!ok) {
    printf("DIRTEST: directory%s (%s), got %d lines, expected %d:\n", timed_out ? " at timeout" : "",
           in_image ? "D64" : "FAT root", n_got, n_expect);
    for (int i = 0; i < n_got || i < n_expect; i++) {
      const char* g = i < n_got ? got[i] : "";
      const char* e = i < n_expect ? expect[i] : "";
      printf("  %s %-32s | %s\n", strcmp(g, e) == 0 ? " " : "!", g, e);
    }
    printf("DIRTEST: screen:\n");
    for (int y = 0; y < 25; y++) printf("  |%s\n", screen[y]);
  }
  return ok;
}
#endif

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
  RP2350Options options = {.coreArch = "arm", .loadFirmware = NULL};
  mcu = RP2350_new(&options);
  open_sd_card();
  init_board_inputs();
  if (sd_card != NULL) {
    mcu->spi[SD_SPI_NUM]->onTransmit_fn = on_spi_transmit;
    mcu->spi[SD_SPI_NUM]->onTransmit_ctx = mcu->spi[SD_SPI_NUM];
  }

  const char* firmware_path = getenv("SD2IEC_UF2");
  if (firmware_path == NULL) {
    firmware_path = FIRMWARE_PATH;
  }
  printf("Loading firmware %s\n", firmware_path);
  RP2350_loadFirmware(mcu, firmware_path, NULL);

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

  const bool show_pc = env_int("RP2_SHOW_PC", 0) != 0;
  iec_trace = env_int("IEC_TRACE", 0) != 0;

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
      if (show_pc) {
        const uint8_t bus = c64_get_iec_bus();
        printf("core0 PC 0x%08x  C64 PC $%04x  IEC bus ATN=%d CLK=%d DATA=%d (1 = released)\n",
               (unsigned)CortexM33Core_PC_get(mcu->core[0].obj), c64_get_pc(), !!(bus & IECLINE_ATN),
               !!(bus & IECLINE_CLK), !!(bus & IECLINE_DATA));
      }
      fflush(stdout);

      motor_animation_index = (motor_animation_index + 1) % motor_anim_len;

      hz_needed_len = 0;
      keep_running = STOP_AFTER_SYSTEM_SECONDS <= 0.0 || c64_seconds < STOP_AFTER_SYSTEM_SECONDS;
    }

    const bool must_tick_c64 = floor(tick_count_c64) != floor(last_tick_count_c64);
    const bool must_tick_c1541 = floor(tick_count_c1541) != floor(last_tick_count_c1541);

    if (must_tick_c64) {
      iec_trace_time_us = tick_count_c64 / C64_TICKS_PER_SECOND * 1.0e6;
      tick_c64();
    }

#ifdef AUTOTEST
    /* Check cartridge completion marker at $C100 */
    if (must_tick_c64) {
      uint8_t prog = c64_ram_read(0xC100);
      bool timeout = tick_count_c64 > 2300000;
      if (prog == 0xFF || prog == 0xEE || timeout) {
        static const uint8_t expected[256] = {
            0x12,0x01,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x15,0xFF,0xFF,0x1F,0x15,0xFF,0xFF,0x1F,0x15,0xFF,0xFF,0x1F,0x15,0xFF,0xFF,0x1F,0x15,0xFF,0xFF,0x1F,0x15,0xFF,0xFF,0x1F,0x07,0x30,0xE2,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x6C,0xFB,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0xF0,0xE3,0x03,0x12,0xFF,0xFF,0x03,0x12,0xFF,0xFF,0x03,0x12,0xFF,0xFF,0x03,0x12,0xFF,0xFF,0x03,0x12,0xFF,0xFF,0x03,0x11,0xFF,0xFF,0x01,0x11,0xFF,0xFF,0x01,0x11,0xFF,0xFF,0x01,0x11,0xFF,0xFF,0x01,0x11,0xFF,0xFF,0x01,0x54,0x45,0x53,0x54,0x2F,0x44,0x45,0x4D,0x4F,0x20,0x20,0x31,0x2F,0x38,0x35,0xA0,0xA0,0xA0,0x38,0x34,0xA0,0x32,0x41,0xA0,0xA0,0xA0,0xA0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        };
        int pass = (prog == 0xFF);
        if (pass) {
          for (int i = 0; i < 256; i++) {
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
          for (int i = 0; i < 256; i++) printf(" %02X", c64_ram_read(0xC000 + i));
          printf("\n  Expected:    ");
          for (int i = 0; i < 256; i++) printf(" %02X", expected[i]);
          printf("\n");
        }
        free(hz_needed);
        return pass ? 0 : 1;
      }
    }
#endif

#ifdef DIRTEST
    /* Check the LOAD"$",8 / LIST output (typed by the wrapper) every 0.1 s of C64 time */
    if (must_tick_c64 && fmod(floor(tick_count_c64), 98525.0) == 0.0) {
      const int result = check_dir_listing(tick_count_c64 / C64_TICKS_PER_SECOND > DIRTEST_TIMEOUT_SECONDS);
      if (result >= 0) {
        printf("DIRTEST: %s (%.2fs C64, %.0f C1541 ticks)\n", result ? "PASS" : "FAIL",
               tick_count_c64 / C64_TICKS_PER_SECOND, tick_count_c1541);
        free(hz_needed);
        return result ? 0 : 1;
      }
    }
#endif

    if (must_tick_c1541) {
      /* The sd2iec firmware has no "tick done" trace marker, so run the
       * simulated RP2 for one C1541 microsecond (150 cycles at 150MHz). */
      const int64_t start_cycles = RP2350_cycles_get(mcu);
      while ((RP2350_cycles_get(mcu) - start_cycles) < RP2_CYCLES_PER_C1541_TICK) {
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
