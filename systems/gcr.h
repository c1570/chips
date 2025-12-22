//
// Created by crazydee on 22.12.25.
//

#ifndef Z_GCR_H
#define Z_GCR_H

inline bool gcr_file_valid(uint8_t *gcr_data);
inline uint8_t gcr_get_half_track_count(uint8_t *gcr_data);
inline uint16_t gcr_get_max_track_size(uint8_t *gcr_data);
inline bool gcr_get_half_track_bytes(uint8_t *dst, uint8_t *gcr_data, uint8_t half_track);
inline uint8_t gcr_get_half_track_speed_index(uint8_t *gcr_data, uint8_t half_track);
inline uint8_t gcr_full_track_to_half_track(uint8_t full_track);

#endif //Z_GCR_H

#ifdef CHIPS_IMPL

bool gcr_file_valid(uint8_t *gcr_data) {
    const uint8_t *signature = (uint8_t *)"GCR-1541\0";
    for (uint8_t i=0; i < 9; i++) {
        if (gcr_data[i] != signature[i]) {
            return false;
        }
    }
    return true;
}

uint8_t gcr_get_half_track_count(uint8_t *gcr_data) {
    return gcr_data[9];
}

uint16_t gcr_get_max_track_size(uint8_t *gcr_data) {
    return ((uint16_t)gcr_data[0xa]) + ((uint16_t)gcr_data[0xb] << 8);
}

bool gcr_get_half_track_bytes(uint8_t *dst, uint8_t *gcr_data, const uint8_t half_track) {
    const uint16_t size = gcr_get_max_track_size(gcr_data);
    const uint8_t half_tracks = gcr_get_half_track_count(gcr_data);
    if (half_track < 1 || half_track > half_tracks) {
        return false;
    }
    const uint32_t half_track_data_offset = ((uint32_t *)(gcr_data + 0xc))[half_track - 1];
    for (uint16_t i=0; i < size; i++) {
        dst[i] = gcr_data[half_track_data_offset+i];
    }
    dst[size] = 0; // mark track done, protection against memory garbage
    return true;
}

uint8_t gcr_get_half_track_speed_index(uint8_t *gcr_data, const uint8_t half_track) {
    const unsigned int half_tracks = gcr_get_half_track_count(gcr_data);
    if (half_track < 1 || half_track > half_tracks) {
        return 0;
    }
    return (gcr_data + 0xc + (half_tracks << 2))[half_track - 1];
}

uint8_t gcr_full_track_to_half_track(const uint8_t full_track) {
    return (full_track << 1) - 1; // 1 -> 1, 2 -> 3, 3 -> 5, ...
}

#endif
