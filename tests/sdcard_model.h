/*
 * SPI-mode SD card model for the rp2350js test runner.
 *
 * Byte-level: the host calls sdcard_xfer() once per SPI byte with the current
 * chip-select level and the MOSI byte, and gets the MISO byte back. The card
 * content is an in-memory copy of a raw image file (e.g. made by mkfs.fat).
 */
#ifndef SDCARD_MODEL_H
#define SDCARD_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct sdcard sdcard_t;

typedef struct {
  /* SDSC: byte addressing and CSD v1. Default (false) is SDHC: block addressing, CSD v2. */
  bool sdsc;
  /* 0xFF bytes between the end of a command and its R1 (spec: 0..8 for NCR beyond the first) */
  int ncr;
  /* 0xFF bytes between R1 and the 0xFE data token of a read */
  int nac;
  /* Number of ACMD41 polls answered with "still idle" */
  int acmd41_busy_polls;
  /* 0x00 busy bytes after an accepted write block */
  int write_busy;
  /* Trace to stderr: 1 = one line per command, 2 = also every SPI byte */
  int trace;
  /* Fault injection, 1-based, 0 = off: corrupt the data CRC of the n-th read block */
  int fault_read_crc_nth;
  /* Fault injection, 1-based, 0 = off: answer the n-th command after init with a CRC error */
  int fault_cmd_crc_nth;
} sdcard_opts_t;

/* Defaults that resemble a real card */
sdcard_opts_t sdcard_default_opts(void);

/* Load the image at @path. Returns NULL (and prints why) on failure. */
sdcard_t* sdcard_open(const char* path, const sdcard_opts_t* opts);

/* Create a card backed by a copy of @data (for tests). */
sdcard_t* sdcard_open_mem(const uint8_t* data, uint32_t size, const sdcard_opts_t* opts);

void sdcard_close(sdcard_t* card);

/* One SPI byte. @cs_active is true while the card's chip select is asserted (low). */
uint8_t sdcard_xfer(sdcard_t* card, bool cs_active, uint8_t mosi);

/* Raw access to the card content, e.g. to check writes. */
uint8_t* sdcard_data(sdcard_t* card);
uint32_t sdcard_sector_count(const sdcard_t* card);

/* Write the (possibly modified) content to @path. Returns 0 on success. */
int sdcard_save(const sdcard_t* card, const char* path);

/* CRC helpers, same polynomials sd2iec uses (exposed for tests) */
uint8_t sdcard_crc7(const uint8_t* data, int len);
uint16_t sdcard_crc16(const uint8_t* data, int len);

#endif
