/*
 * Host-only tests for sdcard_model.c.
 *
 * The host side mirrors the byte sequences of sd2iec/src/sdcard.c (send_command,
 * sd_initialize, sd_read, sd_write, sd_getinfo) so the model is checked against
 * what the firmware actually sends.
 */
#include "sdcard_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, ...)                                     \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("  FAIL %s:%d: ", __FILE__, __LINE__);          \
      printf(__VA_ARGS__);                                   \
      printf("\n");                                          \
      failures++;                                            \
    }                                                        \
  } while (0)

/* ── Host side, mirroring sd2iec's sdcard.c ─────────────────────────────────── */

static sdcard_t* card;
static bool cs_low;

static uint8_t xfer(uint8_t b) { return sdcard_xfer(card, cs_low, b); }
static void select_card(bool on) { cs_low = on; }
static uint8_t rx_byte(void) { return xfer(0xff); }

static void deselect(void) {
  select_card(false);
  rx_byte();
  rx_byte();
  rx_byte();
}

static uint8_t send_command(uint8_t cmd, uint32_t arg, bool good_crc) {
  uint8_t frame[6] = {cmd, (uint8_t)(arg >> 24), (uint8_t)(arg >> 16), (uint8_t)(arg >> 8), (uint8_t)arg, 0};
  frame[5] = (uint8_t)((sdcard_crc7(frame, 5) << 1) | 1);
  if (!good_crc) frame[5] ^= 0x02;

  for (int attempt = 0; attempt < 10; attempt++) {
    select_card(true);
    for (int i = 0; i < 6; i++) xfer(frame[i]);
    uint8_t res;
    int polls = 0;
    do {
      res = rx_byte();
    } while ((res & 0x80) && ++polls < 100);
    if ((res & 0x08) && good_crc) { /* CRC error: retry like sd2iec */
      deselect();
      continue;
    }
    return res;
  }
  return 0xff;
}

static bool sd2iec_init(bool* sdhc) {
  select_card(false);
  for (int i = 0; i < 10; i++) xfer(0xff);

  uint8_t res = send_command(0x40, 0, true);
  deselect();
  if (res != 1) return false;

  res = send_command(0x48, 0x1aa, true);
  if (res == 1) {
    uint8_t r7[4];
    for (int i = 0; i < 4; i++) r7[i] = rx_byte();
    deselect();
    if ((r7[2] & 0x0f) != 1 || r7[3] != 0xaa) return false;
  } else {
    deselect();
  }

  int loops = 0;
  do {
    res = send_command(0x77, 0, true);
    deselect();
    if (res != 1) break;
    res = send_command(0x69, 1u << 30, true);
    deselect();
  } while (res == 1 && ++loops < 100);
  if (res != 0) return false;

  res = send_command(0x7a, 0, true);
  uint8_t ocr[4];
  for (int i = 0; i < 4; i++) ocr[i] = rx_byte();
  deselect();
  *sdhc = (ocr[0] & 0x40) != 0;

  res = send_command(0x41, 1u << 30, true); /* CMD1, sent to SD cards too */
  deselect();
  if (res != 0) return false;
  res = send_command(0x7b, 1, true); /* CRC on */
  deselect();
  if (res > 1) return false;
  res = send_command(0x50, 512, true);
  deselect();
  return res == 0;
}

static uint16_t crc16(const uint8_t* d, int n) { return sdcard_crc16(d, n); }

/* Returns 0 on success, 1 on CRC mismatch, -1 on protocol error */
static int sd2iec_read_once(uint32_t sector, bool sdhc, uint8_t* buf) {
  uint8_t res = send_command(0x51, sdhc ? sector : sector * 512, true);
  if (res != 0) {
    deselect();
    return -1;
  }
  int polls = 0;
  uint8_t b;
  do {
    b = rx_byte();
  } while (b != 0xfe && ++polls < 1000);
  if (b != 0xfe) {
    deselect();
    return -1;
  }
  for (int i = 0; i < 512; i++) buf[i] = rx_byte();
  uint16_t recv = (uint16_t)(rx_byte() << 8);
  recv |= rx_byte();
  deselect();
  return recv == crc16(buf, 512) ? 0 : 1;
}

static int sd2iec_write(uint32_t sector, bool sdhc, const uint8_t* buf, bool good_crc) {
  uint8_t res = send_command(0x58, sdhc ? sector : sector * 512, true);
  if (res != 0) {
    deselect();
    return -1;
  }
  xfer(0xfe);
  for (int i = 0; i < 512; i++) xfer(buf[i]);
  uint16_t crc = crc16(buf, 512);
  if (!good_crc) crc ^= 1;
  xfer((uint8_t)(crc >> 8));
  xfer((uint8_t)crc);
  res = rx_byte();
  if ((res & 0x0f) != 0x05) {
    deselect();
    return 1;
  }
  int polls = 0;
  do {
    res = rx_byte();
  } while (res == 0 && ++polls < 1000);
  deselect();
  return 0;
}

/* Read the CSD the robust way (wait for 0xFE) and return the sector count */
static uint32_t read_capacity(bool sdhc) {
  uint8_t res = send_command(0x49, 0, true);
  if (res != 0) return 0;
  uint8_t b;
  int polls = 0;
  do {
    b = rx_byte();
  } while (b != 0xfe && ++polls < 100);
  uint8_t csd[18];
  for (int i = 0; i < 18; i++) csd[i] = rx_byte();
  deselect();
  CHECK(((csd[15] >> 1) & 0x7f) == sdcard_crc7(csd, 15), "CSD CRC7");
  if (sdhc) {
    uint32_t c_size = ((uint32_t)(csd[7] & 0x3f) << 16) | (uint32_t)csd[8] << 8 | csd[9];
    return (c_size + 1) * 1024;
  }
  uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) | (uint32_t)csd[7] << 2 | csd[8] >> 6;
  uint32_t mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
  uint32_t bl_len = csd[5] & 0x0f;
  return (c_size + 1) << (mult + 2 + bl_len - 9);
}

/* ── Fixtures ───────────────────────────────────────────────────────────────── */

#define TEST_SECTORS 4096 /* 2 MiB, a multiple of the 512 KiB CSD v2 unit */

static uint8_t* make_image(void) {
  uint8_t* img = malloc((size_t)TEST_SECTORS * 512);
  for (uint32_t s = 0; s < TEST_SECTORS; s++)
    for (int i = 0; i < 512; i++) img[(size_t)s * 512 + i] = (uint8_t)(s * 7 + i * 13 + (s >> 8));
  return img;
}

static void open_card(const uint8_t* img, sdcard_opts_t opts) {
  sdcard_close(card);
  card = sdcard_open_mem(img, (uint32_t)TEST_SECTORS * 512, &opts);
  cs_low = false;
}

/* ── Tests ──────────────────────────────────────────────────────────────────── */

static void test_init_and_read(const uint8_t* img, bool sdsc) {
  printf("init + read (%s)\n", sdsc ? "SDSC" : "SDHC");
  sdcard_opts_t o = sdcard_default_opts();
  o.sdsc = sdsc;
  open_card(img, o);

  bool sdhc = false;
  CHECK(sd2iec_init(&sdhc), "init failed");
  CHECK(sdhc == !sdsc, "card type detected as %s", sdhc ? "SDHC" : "SDSC");

  uint8_t buf[512];
  const uint32_t sectors[] = {0, 1, 63, 2048, TEST_SECTORS - 1};
  for (size_t i = 0; i < sizeof(sectors) / sizeof(sectors[0]); i++) {
    CHECK(sd2iec_read_once(sectors[i], sdhc, buf) == 0, "read sector %u", sectors[i]);
    CHECK(memcmp(buf, img + (size_t)sectors[i] * 512, 512) == 0, "data of sector %u", sectors[i]);
  }
  CHECK(sd2iec_read_once(TEST_SECTORS, sdhc, buf) == -1, "read past the end must fail");
  CHECK(read_capacity(sdhc) == TEST_SECTORS, "capacity from CSD");
}

static void test_write(const uint8_t* img) {
  printf("write\n");
  open_card(img, sdcard_default_opts());
  bool sdhc;
  CHECK(sd2iec_init(&sdhc), "init failed");

  uint8_t out[512], in[512];
  for (int i = 0; i < 512; i++) out[i] = (uint8_t)(255 - i);
  CHECK(sd2iec_write(100, sdhc, out, true) == 0, "write accepted");
  CHECK(sd2iec_read_once(100, sdhc, in) == 0 && memcmp(in, out, 512) == 0, "read back");

  uint8_t bad[512];
  memset(bad, 0x55, sizeof(bad));
  CHECK(sd2iec_write(101, sdhc, bad, false) == 1, "write with bad CRC rejected");
  CHECK(memcmp(sdcard_data(card) + 101 * 512, img + 101 * 512, 512) == 0, "rejected write left data alone");
}

static void test_crc_checks(const uint8_t* img) {
  printf("command CRC\n");
  open_card(img, sdcard_default_opts());
  select_card(false);
  for (int i = 0; i < 10; i++) xfer(0xff);
  uint8_t res = send_command(0x40, 0, false);
  deselect();
  CHECK(res != 0xff && (res & 0x08), "CMD0 with a bad CRC must report a CRC error, got 0x%02x", res);

  bool sdhc;
  open_card(img, sdcard_default_opts());
  CHECK(sd2iec_init(&sdhc), "init failed");
  res = send_command(0x50, 512, false);
  deselect();
  CHECK(res & 0x08, "bad CRC after CMD59 must report a CRC error, got 0x%02x", res);
}

static void test_fault_injection(const uint8_t* img) {
  printf("fault injection\n");
  sdcard_opts_t o = sdcard_default_opts();
  o.fault_read_crc_nth = 2;
  open_card(img, o);
  bool sdhc;
  CHECK(sd2iec_init(&sdhc), "init failed");
  uint8_t buf[512];
  CHECK(sd2iec_read_once(5, sdhc, buf) == 0, "1st read clean");
  CHECK(sd2iec_read_once(5, sdhc, buf) == 1, "2nd read has a CRC error");
  CHECK(sd2iec_read_once(5, sdhc, buf) == 0, "retry is clean");

  o = sdcard_default_opts();
  o.fault_cmd_crc_nth = 1;
  open_card(img, o);
  CHECK(sd2iec_init(&sdhc), "init with an injected command CRC error (retried)");
  CHECK(sd2iec_read_once(7, sdhc, buf) == 0, "read after retry");
}

static void test_multi_block_read(const uint8_t* img) {
  printf("CMD18 + CMD12\n");
  open_card(img, sdcard_default_opts());
  bool sdhc;
  CHECK(sd2iec_init(&sdhc), "init failed");

  CHECK(send_command(0x52, 10, true) == 0, "CMD18 accepted");
  uint8_t buf[512];
  for (int blk = 0; blk < 3; blk++) {
    int polls = 0;
    uint8_t b;
    do {
      b = rx_byte();
    } while (b != 0xfe && ++polls < 100);
    for (int i = 0; i < 512; i++) buf[i] = rx_byte();
    uint16_t crc = (uint16_t)(rx_byte() << 8);
    crc |= rx_byte();
    CHECK(memcmp(buf, img + (size_t)(10 + blk) * 512, 512) == 0, "block %d data", blk);
    CHECK(crc == crc16(buf, 512), "block %d CRC", blk);
  }
  uint8_t stop[6] = {0x4c, 0, 0, 0, 0, 0};
  stop[5] = (uint8_t)((sdcard_crc7(stop, 5) << 1) | 1);
  for (int i = 0; i < 6; i++) xfer(stop[i]);
  uint8_t res;
  int polls = 0;
  do {
    res = rx_byte();
  } while ((res & 0x80) && ++polls < 2000);
  CHECK(res == 0, "CMD12 R1 0x%02x", res);
  deselect();
  CHECK(sd2iec_read_once(20, sdhc, buf) == 0, "single read after CMD12");
}

int main(void) {
  uint8_t* img = make_image();
  test_init_and_read(img, false);
  test_init_and_read(img, true);
  test_write(img);
  test_crc_checks(img);
  test_fault_injection(img);
  test_multi_block_read(img);
  sdcard_close(card);
  free(img);
  printf(failures ? "FAILED: %d check(s)\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
