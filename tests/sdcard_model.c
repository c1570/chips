/*
 * SPI-mode SD card model, see sdcard_model.h.
 *
 * Supported commands: CMD0, 1, 8, 9, 10, 12, 13, 16, 17, 18, 24, 25, 55, 58, 59 and ACMD41.
 * Anything else is answered with R1 ILLEGAL_COMMAND.
 */
#include "sdcard_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512

/* R1 bits */
#define R1_IDLE 0x01
#define R1_ILLEGAL_COMMAND 0x04
#define R1_CRC_ERROR 0x08
#define R1_ADDRESS_ERROR 0x20
#define R1_PARAMETER_ERROR 0x40

#define TOKEN_START_BLOCK 0xfe
#define TOKEN_START_MULTI_WRITE 0xfc
#define TOKEN_STOP_MULTI_WRITE 0xfd

#define DATA_RESPONSE_ACCEPTED 0x05
#define DATA_RESPONSE_CRC_ERROR 0x0b

/* Output queue: large enough for R1 + gap + token + 512 bytes + CRC */
#define OUTQ_SIZE 1024

typedef enum {
  RX_COMMAND,     /* waiting for / collecting a 6-byte command frame */
  RX_WRITE_TOKEN, /* waiting for the start token of a write block */
  RX_WRITE_DATA,  /* collecting 512 data bytes + 2 CRC bytes */
} rx_state_t;

struct sdcard {
  sdcard_opts_t opts;
  uint8_t* data;
  uint32_t sectors;

  bool spi_mode;  /* CMD0 seen with CS asserted */
  bool idle;      /* still in idle state, ACMD41/CMD1 not completed */
  bool app_cmd;   /* last command was CMD55 */
  bool crc_on;    /* CMD59 */
  int init_polls; /* remaining ACMD41/CMD1 polls that report idle */
  bool last_cs;

  rx_state_t rx;
  uint8_t cmd[6];
  int cmd_len;

  /* write in progress */
  bool multi_write;
  uint32_t write_sector;
  uint8_t write_buf[SECTOR_SIZE + 2];
  int write_len;

  /* multi-block read in progress */
  bool multi_read;
  uint32_t read_sector;

  uint8_t outq[OUTQ_SIZE];
  int out_head;
  int out_len;

  uint32_t commands_after_init;
  uint32_t blocks_read;
};

/* ── CRCs ───────────────────────────────────────────────────────────────────── */

uint8_t sdcard_crc7(const uint8_t* data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    uint8_t d = data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc <<= 1;
      if (d & 0x80) crc ^= 0x80;
      if (crc & 0x80) crc ^= 0x09;
      d <<= 1;
    }
  }
  return crc & 0x7f;
}

uint16_t sdcard_crc16(const uint8_t* data, int len) {
  uint16_t crc = 0;
  for (int i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int bit = 0; bit < 8; bit++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

/* ── Output queue ───────────────────────────────────────────────────────────── */

static void out_push(sdcard_t* c, uint8_t b) {
  if (c->out_len == OUTQ_SIZE) {
    fprintf(stderr, "sdcard_model: output queue overflow\n");
    abort();
  }
  c->outq[(c->out_head + c->out_len) % OUTQ_SIZE] = b;
  c->out_len++;
}

static void out_fill(sdcard_t* c, uint8_t b, int count) {
  for (int i = 0; i < count; i++) out_push(c, b);
}

static uint8_t out_pop(sdcard_t* c) {
  uint8_t b = c->outq[c->out_head];
  c->out_head = (c->out_head + 1) % OUTQ_SIZE;
  c->out_len--;
  return b;
}

static void out_clear(sdcard_t* c) {
  c->out_head = 0;
  c->out_len = 0;
}

/* Queue a data block: Nac gap, start token, payload, CRC16 */
static void out_data_block(sdcard_t* c, const uint8_t* payload, int len, bool corrupt_crc) {
  out_fill(c, 0xff, c->opts.nac);
  out_push(c, TOKEN_START_BLOCK);
  for (int i = 0; i < len; i++) out_push(c, payload[i]);
  uint16_t crc = sdcard_crc16(payload, len);
  if (corrupt_crc) crc ^= 0x5a5a;
  out_push(c, (uint8_t)(crc >> 8));
  out_push(c, (uint8_t)crc);
}

/* ── Registers ──────────────────────────────────────────────────────────────── */

/* Set @nbits of a 128-bit big-endian register, @msb is the register bit number (127..0) */
static void reg_set_bits(uint8_t reg[16], int msb, int nbits, uint32_t value) {
  for (int i = 0; i < nbits; i++) {
    int bitno = msb - i; /* 127 is the MSB of reg[0] */
    int byte = 15 - bitno / 8;
    int shift = bitno % 8;
    uint32_t v = (value >> (nbits - 1 - i)) & 1;
    reg[byte] = (uint8_t)((reg[byte] & ~(1u << shift)) | (v << shift));
  }
}

static void reg_finish_crc(uint8_t reg[16]) {
  reg[15] = (uint8_t)((sdcard_crc7(reg, 15) << 1) | 1);
}

static void build_csd(const sdcard_t* c, uint8_t csd[16]) {
  memset(csd, 0, 16);
  reg_set_bits(csd, 119, 8, 0x0e);  /* TAAC */
  reg_set_bits(csd, 103, 8, 0x32);  /* TRAN_SPEED: 25 MHz */
  reg_set_bits(csd, 95, 12, 0x5b5); /* CCC */
  reg_set_bits(csd, 83, 4, 9);      /* READ_BL_LEN: 512 */
  reg_set_bits(csd, 46, 1, 1);      /* ERASE_BLK_EN */
  reg_set_bits(csd, 45, 7, 0x7f);   /* SECTOR_SIZE */
  reg_set_bits(csd, 28, 3, 2);      /* R2W_FACTOR */
  reg_set_bits(csd, 25, 4, 9);      /* WRITE_BL_LEN: 512 */

  if (!c->opts.sdsc) {
    /* CSD version 2.0: capacity = (C_SIZE + 1) * 512 KiB */
    reg_set_bits(csd, 127, 2, 1);
    uint32_t c_size = c->sectors / 1024;
    reg_set_bits(csd, 69, 22, c_size ? c_size - 1 : 0);
  } else {
    /* CSD version 1.0: capacity = (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) blocks of 512 bytes */
    uint32_t mult = 0;
    while (mult < 7 && (c->sectors >> (mult + 2)) > 4096) mult++;
    uint32_t c_size = c->sectors >> (mult + 2);
    if (c_size > 4096) c_size = 4096;
    reg_set_bits(csd, 127, 2, 0);
    reg_set_bits(csd, 73, 12, c_size ? c_size - 1 : 0);
    reg_set_bits(csd, 49, 3, mult);
  }
  reg_finish_crc(csd);
}

static void build_cid(uint8_t cid[16]) {
  memset(cid, 0, 16);
  cid[0] = 0x00;                 /* MID */
  memcpy(&cid[1], "EM", 2);      /* OID */
  memcpy(&cid[3], "SDEMU", 5);   /* PNM */
  cid[8] = 0x10;                 /* PRV 1.0 */
  cid[9] = 0x12;                 /* PSN */
  cid[10] = 0x34;
  cid[11] = 0x56;
  cid[12] = 0x78;
  cid[13] = 0x01;                /* MDT */
  cid[14] = 0x9a;
  reg_finish_crc(cid);
}

/* ── Command handling ───────────────────────────────────────────────────────── */

static const char* cmd_name(int index, bool app) {
  if (app && index == 41) return "ACMD41";
  switch (index) {
    case 0: return "CMD0 GO_IDLE_STATE";
    case 1: return "CMD1 SEND_OP_COND";
    case 8: return "CMD8 SEND_IF_COND";
    case 9: return "CMD9 SEND_CSD";
    case 10: return "CMD10 SEND_CID";
    case 12: return "CMD12 STOP_TRANSMISSION";
    case 13: return "CMD13 SEND_STATUS";
    case 16: return "CMD16 SET_BLOCKLEN";
    case 17: return "CMD17 READ_SINGLE_BLOCK";
    case 18: return "CMD18 READ_MULTIPLE_BLOCK";
    case 24: return "CMD24 WRITE_BLOCK";
    case 25: return "CMD25 WRITE_MULTIPLE_BLOCK";
    case 55: return "CMD55 APP_CMD";
    case 58: return "CMD58 READ_OCR";
    case 59: return "CMD59 CRC_ON_OFF";
    default: return "unknown";
  }
}

/* Translate a command address into a sector number; false if invalid */
static bool address_to_sector(const sdcard_t* c, uint32_t arg, uint32_t* sector) {
  if (c->opts.sdsc) {
    if (arg % SECTOR_SIZE) return false;
    arg /= SECTOR_SIZE;
  }
  if (arg >= c->sectors) return false;
  *sector = arg;
  return true;
}

static void queue_read_block(sdcard_t* c, uint32_t sector) {
  c->blocks_read++;
  bool corrupt = c->opts.fault_read_crc_nth && c->blocks_read == (uint32_t)c->opts.fault_read_crc_nth;
  if (corrupt && c->opts.trace) fprintf(stderr, "SD: injecting data CRC error on sector %u\n", sector);
  out_data_block(c, c->data + (size_t)sector * SECTOR_SIZE, SECTOR_SIZE, corrupt);
}

/* Handle a complete 6-byte command frame */
static void handle_command(sdcard_t* c) {
  const int index = c->cmd[0] & 0x3f;
  const uint32_t arg = (uint32_t)c->cmd[1] << 24 | (uint32_t)c->cmd[2] << 16 | (uint32_t)c->cmd[3] << 8 | c->cmd[4];
  const bool app = c->app_cmd;
  c->app_cmd = false;

  /* Before CMD0 the card is in SD mode and ignores SPI traffic */
  if (!c->spi_mode && index != 0) {
    if (c->opts.trace) fprintf(stderr, "SD: %s ignored, card not in SPI mode\n", cmd_name(index, app));
    return;
  }

  const bool crc_ok = (c->cmd[5] >> 1) == sdcard_crc7(c->cmd, 5);
  const bool crc_checked = c->crc_on || index == 0 || index == 8;

  uint8_t r1 = c->idle ? R1_IDLE : 0;
  bool injected = false;
  if (!c->idle && c->opts.fault_cmd_crc_nth) {
    c->commands_after_init++;
    injected = c->commands_after_init == (uint32_t)c->opts.fault_cmd_crc_nth;
  }

  out_fill(c, 0xff, c->opts.ncr);

  if ((crc_checked && !crc_ok) || injected) {
    if (c->opts.trace)
      fprintf(stderr, "SD: %s arg=0x%08x -> CRC error%s\n", cmd_name(index, app), arg, injected ? " (injected)" : "");
    out_push(c, r1 | R1_CRC_ERROR);
    return;
  }

  /* Commands a card accepts while still idle */
  const bool allowed_idle = index == 0 || index == 1 || index == 8 || index == 55 || index == 58 || index == 59 ||
                            (app && index == 41);
  if (c->idle && !allowed_idle) {
    if (c->opts.trace) fprintf(stderr, "SD: %s arg=0x%08x -> rejected, card idle\n", cmd_name(index, app), arg);
    out_push(c, r1 | R1_ILLEGAL_COMMAND);
    return;
  }

  uint8_t resp = r1;
  switch (index) {
    case 0:
      c->spi_mode = true;
      c->idle = true;
      c->crc_on = false;
      c->multi_read = false;
      c->init_polls = c->opts.acmd41_busy_polls;
      resp = R1_IDLE;
      out_push(c, resp);
      break;

    case 1:
    case 41:
      if (index == 41 && !app) goto illegal;
      if (c->idle) {
        if (c->init_polls > 0)
          c->init_polls--;
        else
          c->idle = false;
      }
      resp = c->idle ? R1_IDLE : 0;
      out_push(c, resp);
      break;

    case 8:
      out_push(c, resp);
      out_push(c, 0x00);
      out_push(c, 0x00);
      out_push(c, (uint8_t)((arg >> 8) & 0x0f)); /* voltage accepted */
      out_push(c, (uint8_t)arg);                 /* check pattern echo */
      break;

    case 9:
    case 10: {
      uint8_t reg[16];
      if (index == 9)
        build_csd(c, reg);
      else
        build_cid(reg);
      out_push(c, resp);
      out_data_block(c, reg, 16, false);
      break;
    }

    case 12:
      /* R1b: a stuff byte, R1, then a short busy */
      c->multi_read = false;
      out_clear(c);
      out_push(c, 0xff);
      out_push(c, resp);
      out_push(c, 0x00);
      break;

    case 13:
      out_push(c, resp);
      out_push(c, 0x00);
      break;

    case 16:
      if (arg != SECTOR_SIZE) resp |= R1_PARAMETER_ERROR;
      out_push(c, resp);
      break;

    case 17:
    case 18: {
      uint32_t sector;
      if (!address_to_sector(c, arg, &sector)) {
        resp |= R1_ADDRESS_ERROR;
        out_push(c, resp);
        break;
      }
      out_push(c, resp);
      queue_read_block(c, sector);
      if (index == 18) {
        c->multi_read = true;
        c->read_sector = sector + 1;
      }
      break;
    }

    case 24:
    case 25: {
      uint32_t sector;
      if (!address_to_sector(c, arg, &sector)) {
        resp |= R1_ADDRESS_ERROR;
        out_push(c, resp);
        break;
      }
      out_push(c, resp);
      c->write_sector = sector;
      c->multi_write = index == 25;
      c->rx = RX_WRITE_TOKEN;
      break;
    }

    case 55:
      c->app_cmd = true;
      out_push(c, resp);
      break;

    case 58: {
      /* OCR: bit 31 power-up done, bit 30 CCS, 3.2-3.4 V window */
      uint8_t top = 0;
      if (!c->idle) top = (uint8_t)(0x80 | (c->opts.sdsc ? 0 : 0x40));
      out_push(c, resp);
      out_push(c, top);
      out_push(c, 0xff);
      out_push(c, 0x80);
      out_push(c, 0x00);
      break;
    }

    case 59:
      c->crc_on = arg & 1;
      out_push(c, resp);
      break;

    default:
    illegal:
      resp |= R1_ILLEGAL_COMMAND;
      out_push(c, resp);
      break;
  }

  if (c->opts.trace) fprintf(stderr, "SD: %s arg=0x%08x -> R1 0x%02x\n", cmd_name(index, app), arg, resp);
}

static void handle_write_byte(sdcard_t* c, uint8_t mosi) {
  if (c->rx == RX_WRITE_TOKEN) {
    if (c->multi_write && mosi == TOKEN_STOP_MULTI_WRITE) {
      if (c->opts.trace) fprintf(stderr, "SD: multi-block write stopped\n");
      out_push(c, 0xff);
      out_fill(c, 0x00, c->opts.write_busy);
      c->rx = RX_COMMAND;
      return;
    }
    if (mosi == (c->multi_write ? TOKEN_START_MULTI_WRITE : TOKEN_START_BLOCK)) {
      c->rx = RX_WRITE_DATA;
      c->write_len = 0;
    }
    return;
  }

  /* RX_WRITE_DATA */
  c->write_buf[c->write_len++] = mosi;
  if (c->write_len < SECTOR_SIZE + 2) return;

  const uint16_t received = (uint16_t)(c->write_buf[SECTOR_SIZE] << 8 | c->write_buf[SECTOR_SIZE + 1]);
  const bool crc_ok = !c->crc_on || received == sdcard_crc16(c->write_buf, SECTOR_SIZE);
  if (crc_ok) {
    memcpy(c->data + (size_t)c->write_sector * SECTOR_SIZE, c->write_buf, SECTOR_SIZE);
    out_push(c, DATA_RESPONSE_ACCEPTED);
    out_fill(c, 0x00, c->opts.write_busy);
  } else {
    out_push(c, DATA_RESPONSE_CRC_ERROR);
  }
  if (c->opts.trace)
    fprintf(stderr, "SD: write sector %u %s\n", c->write_sector, crc_ok ? "accepted" : "rejected, CRC error");

  if (c->multi_write && crc_ok && c->write_sector + 1 < c->sectors) {
    c->write_sector++;
    c->rx = RX_WRITE_TOKEN;
  } else {
    c->rx = RX_COMMAND;
  }
}

static uint8_t xfer(sdcard_t* c, bool cs_active, uint8_t mosi);

uint8_t sdcard_xfer(sdcard_t* c, bool cs_active, uint8_t mosi) {
  uint8_t miso = xfer(c, cs_active, mosi);
  if (c->opts.trace >= 2) fprintf(stderr, "SD: %s %02x -> %02x\n", cs_active ? "CS" : "--", mosi, miso);
  return miso;
}

static uint8_t xfer(sdcard_t* c, bool cs_active, uint8_t mosi) {
  /* Deselect ends any transaction in progress */
  if (!cs_active) {
    if (c->last_cs) {
      out_clear(c);
      c->rx = RX_COMMAND;
      c->cmd_len = 0;
      c->multi_read = false;
    }
    c->last_cs = false;
    return 0xff;
  }
  c->last_cs = true;

  /* MISO is what the card already had queued when this byte started */
  uint8_t miso = 0xff;
  if (c->out_len > 0) miso = out_pop(c);

  switch (c->rx) {
    case RX_WRITE_TOKEN:
    case RX_WRITE_DATA:
      if (c->out_len == 0) handle_write_byte(c, mosi);
      break;

    case RX_COMMAND:
      /* A card that is still sending only listens for CMD12 during a multi-block read */
      if (c->out_len > 0 && !c->multi_read) break;
      if (c->cmd_len == 0 && (mosi & 0xc0) != 0x40) break;
      c->cmd[c->cmd_len++] = mosi;
      if (c->cmd_len == 6) {
        c->cmd_len = 0;
        const bool is_stop = (c->cmd[0] & 0x3f) == 12;
        if (c->multi_read && !is_stop) break; /* ignore anything but CMD12 */
        handle_command(c);
      }
      break;
  }

  /* Keep a multi-block read streaming */
  if (c->multi_read && c->out_len == 0) {
    if (c->read_sector < c->sectors) {
      queue_read_block(c, c->read_sector++);
    } else {
      c->multi_read = false;
    }
  }

  return miso;
}

/* ── Lifecycle ──────────────────────────────────────────────────────────────── */

sdcard_opts_t sdcard_default_opts(void) {
  sdcard_opts_t o = {0};
  o.ncr = 1;
  o.nac = 2;
  o.acmd41_busy_polls = 2;
  o.write_busy = 4;
  return o;
}

sdcard_t* sdcard_open_mem(const uint8_t* data, uint32_t size, const sdcard_opts_t* opts) {
  if (size < SECTOR_SIZE) {
    fprintf(stderr, "sdcard_model: image too small (%u bytes)\n", size);
    return NULL;
  }
  sdcard_t* c = calloc(1, sizeof(*c));
  if (!c) return NULL;
  c->opts = opts ? *opts : sdcard_default_opts();
  c->sectors = size / SECTOR_SIZE;
  c->data = malloc((size_t)c->sectors * SECTOR_SIZE);
  if (!c->data) {
    free(c);
    return NULL;
  }
  memcpy(c->data, data, (size_t)c->sectors * SECTOR_SIZE);
  return c;
}

sdcard_t* sdcard_open(const char* path, const sdcard_opts_t* opts) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    perror(path);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size <= 0 || size > 0x7fffffffL) {
    fprintf(stderr, "sdcard_model: unsupported image size %ld\n", size);
    fclose(f);
    return NULL;
  }
  uint8_t* buf = malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
    fprintf(stderr, "sdcard_model: cannot read %s\n", path);
    free(buf);
    fclose(f);
    return NULL;
  }
  fclose(f);
  sdcard_t* c = sdcard_open_mem(buf, (uint32_t)size, opts);
  free(buf);
  if (c && c->opts.trace)
    fprintf(stderr, "SD: %s, %u sectors, %s\n", path, c->sectors, c->opts.sdsc ? "SDSC" : "SDHC");
  return c;
}

void sdcard_close(sdcard_t* c) {
  if (!c) return;
  free(c->data);
  free(c);
}

uint8_t* sdcard_data(sdcard_t* c) { return c->data; }

uint32_t sdcard_sector_count(const sdcard_t* c) { return c->sectors; }

int sdcard_save(const sdcard_t* c, const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return -1;
  size_t n = fwrite(c->data, SECTOR_SIZE, c->sectors, f);
  fclose(f);
  return n == c->sectors ? 0 : -1;
}
