#pragma once
/*
    disk_helpers.h

    D64 to GCR conversion helper functions for Commodore 1541 disk emulation.

    D64 files contain raw 256-byte sectors that need to be converted to GCR
    (Group Code Recording) format for the 1541 drive to read them.

    Based on conversion logic from nibtools.
*/

#include <stdint.h>
#include <string.h>

// D64 constants
#define MAX_TRACKS_1541 42
#define SYNC_LENGTH      5
#define HEADER_LENGTH    10
#define HEADER_GAP_LENGTH 9
#define DATA_LENGTH       325  // 65 * 5 GCR bytes

// DENSITY constants for track capacity (bytes per minute at 4MHz base clock)
#define DENSITY3 2307692
#define DENSITY2 2142857
#define DENSITY1 2000000
#define DENSITY0 1875000

// Sectors per track (1-based index)
// Track 1-17: 21 sectors, Track 18-24: 19 sectors, Track 25-30: 18 sectors, Track 31-42: 17 sectors
static const uint8_t sector_map[MAX_TRACKS_1541 + 1] = {
    0,
    21,21,21,21,21,21,21,21,21,21,    //  1-10
    21,21,21,21,21,21,21,19,19,19,    // 11-20
    19,19,19,19,18,18,18,18,18,18,    // 21-30
    17,17,17,17,17,                    // 31-35
    17,17,17,17,17,17,17               // 36-42
};

// Sector gap length per track (bytes between sectors)
static const uint8_t sector_gap_length[MAX_TRACKS_1541 + 1] = {
    0,
    10,10,10,10,10,10,10,10,10,10,    //  1-10
    10,10,10,10,10,10,10,17,17,17,    // 11-20
    17,17,17,17,11,11,11,11,11,11,    // 21-30
    8,8,8,8,8,                          // 31-35
    8,8,8,8,8,8,8                       // 36-42
};

// Speed zone per track (0=fastest/inner, 3=slowest/outer)
static const uint8_t speed_map[MAX_TRACKS_1541 + 1] = {
    0,
    3,3,3,3,3,3,3,3,3,3,              //  1-10
    3,3,3,3,3,3,3,2,2,2,              // 11-20
    2,2,2,2,1,1,1,1,1,1,              // 21-30
    0,0,0,0,0,                         // 31-35
    0,0,0,0,0,0,0                       // 36-42
};

// Track capacity by speed zone (bytes per track at 300 RPM)
// DENSITY0/300 = ~6250, DENSITY1/300 = ~6666, DENSITY2/300 = ~7142, DENSITY3/300 = ~7692
static const uint16_t track_capacity[] = {
    (uint16_t)(DENSITY0 / 300),   // Density 0: ~6250 bytes
    (uint16_t)(DENSITY1 / 300),   // Density 1: ~6666 bytes
    (uint16_t)(DENSITY2 / 300),   // Density 2: ~7142 bytes
    (uint16_t)(DENSITY3 / 300)    // Density 3: ~7692 bytes
};

// GCR conversion table (4-bit to 5-bit)
// Converts 4 data bits to 5 GCR bits according to Commodore's GCR encoding
static const uint8_t GCR_conv_data[16] = {
    0x0a, 0x0b, 0x12, 0x13,
    0x0e, 0x0f, 0x16, 0x17,
    0x09, 0x19, 0x1a, 0x1b,
    0x0d, 0x1d, 0x1e, 0x15
};

// Convert 4 bytes to 5 GCR bytes
// buffer: pointer to 4 bytes of input data
// ptr: pointer to destination buffer (will write 5 bytes)
static inline void convert_4bytes_to_GCR(const uint8_t *buffer, uint8_t *ptr) {
    *ptr = GCR_conv_data[(*buffer) >> 4] << 3;
    *ptr |= GCR_conv_data[(*buffer) & 0x0f] >> 2;
    ptr++;

    *ptr = GCR_conv_data[(*buffer) & 0x0f] << 6;
    buffer++;
    *ptr |= GCR_conv_data[(*buffer) >> 4] << 1;
    *ptr |= GCR_conv_data[(*buffer) & 0x0f] >> 4;
    ptr++;

    *ptr = GCR_conv_data[(*buffer) & 0x0f] << 4;
    buffer++;
    *ptr |= GCR_conv_data[(*buffer) >> 4] >> 1;
    ptr++;

    *ptr = GCR_conv_data[(*buffer) >> 4] << 7;
    *ptr |= GCR_conv_data[(*buffer) & 0x0f] << 2;
    buffer++;
    *ptr |= GCR_conv_data[(*buffer) >> 4] >> 3;
    ptr++;

    *ptr = GCR_conv_data[(*buffer) >> 4] << 5;
    *ptr |= GCR_conv_data[(*buffer) & 0x0f];
}

// Convert one D64 sector to GCR format
// buffer: 256-byte D64 sector data
// ptr: destination GCR buffer
// track: track number (1-42)
// sector: sector number (0-based)
// diskID: 2-byte disk ID
static inline void convert_sector_to_GCR(const uint8_t *buffer, uint8_t *ptr,
                                          uint8_t track, uint8_t sector,
                                          const uint8_t *diskID) {
    uint8_t buf[4], databuf[0x104], chksum;
    uint16_t sector_size = SYNC_LENGTH + HEADER_LENGTH + HEADER_GAP_LENGTH +
                           SYNC_LENGTH + DATA_LENGTH;

    // Initialize entire sector area with gap bytes (like nibtools does)
    memset(ptr, 0x55, sector_size + sector_gap_length[track]);

    // Header sync (5 bytes of 0xFF)
    memset(ptr, 0xff, SYNC_LENGTH);
    ptr += SYNC_LENGTH;

    // Header data (10 bytes GCR encoded from 4 bytes)
    buf[0] = 0x08;  // Header identifier (header vs data)
    buf[1] = (uint8_t)(sector ^ track ^ diskID[1] ^ diskID[0]);  // Checksum
    buf[2] = (uint8_t)sector;
    buf[3] = (uint8_t)track;

    convert_4bytes_to_GCR(buf, ptr);
    ptr += 5;
    buf[0] = diskID[1];
    buf[1] = diskID[0];
    buf[2] = buf[3] = 0x0f;  // Padding
    convert_4bytes_to_GCR(buf, ptr);
    ptr += 5;

    // Header gap (9 bytes of 0x55)
    memset(ptr, 0x55, HEADER_GAP_LENGTH);
    ptr += HEADER_GAP_LENGTH;

    // Data sync (5 bytes of 0xFF)
    memset(ptr, 0xff, SYNC_LENGTH);
    ptr += SYNC_LENGTH;

    // Data block (325 bytes GCR encoded from 260 bytes)
    // Format: 1 byte identifier + 256 bytes data + 1 byte checksum + 2 bytes padding
    chksum = 0;
    databuf[0] = 0x07;  // Data block identifier
    for (int i = 0; i < 256; i++) {
        databuf[i + 1] = buffer[i];
        chksum ^= buffer[i];
    }
    databuf[0x101] = chksum;
    databuf[0x102] = 0;
    databuf[0x103] = 0;

    // Convert 260 bytes (65 groups of 4) to 325 GCR bytes
    for (int i = 0; i < 65; i++) {
        convert_4bytes_to_GCR(databuf + (4 * i), ptr);
        ptr += 5;
    }

    // Sector gap (variable length based on track)
    memset(ptr, 0x55, sector_gap_length[track]);
}

// Calculate byte offset in D64 file for a given track
// Returns offset from beginning of file to first sector of track
static inline uint32_t d64_track_offset(uint8_t track) {
    uint32_t offset = 0;
    for (uint8_t t = 1; t < track; t++) {
        offset += sector_map[t] * 256;
    }
    return offset;
}

// Get track size in D64 file (for a full track, all sectors)
static inline uint32_t d64_track_size(uint8_t track) {
    if (track < 1 || track > MAX_TRACKS_1541) {
        return 0;
    }
    return sector_map[track] * 256;
}

// Get GCR track size for a given track (including all sectors, sync, headers, gaps)
static inline uint16_t d64_gcr_track_size(uint8_t track) {
    if (track < 1 || track > MAX_TRACKS_1541) {
        return 0;
    }
    uint8_t num_sectors = sector_map[track];
    uint16_t sector_size = SYNC_LENGTH + HEADER_LENGTH + HEADER_GAP_LENGTH +
                           SYNC_LENGTH + DATA_LENGTH;
    return num_sectors * sector_size;
}
