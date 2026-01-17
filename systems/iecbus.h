//#pragma once
/*#
    # iecbus.h

    An emulation for the IEC serial line connection.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    ## zlib/libpng license

    Copyright (c) 2019 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/

#ifndef Z_IECBUS_H
#define Z_IECBUS_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// #define __IEC_DEBUG

#define IECLINE_DATA  (1<<0)
#define IECLINE_CLK   (1<<1)
#define IECLINE_ATN   (1<<2)
#define IECLINE_SRQIN (1<<3)
#define IECLINE_RESET (1<<4)

#define IEC_DATA_ACTIVE(a) (!((a)&IECLINE_DATA))
#define IEC_CLK_ACTIVE(a) (!((a)&IECLINE_CLK))
#define IEC_ATN_ACTIVE(a) (!((a)&IECLINE_ATN))
#define IEC_SRQIN_ACTIVE(a) (!((a)&IECLINE_SRQIN))
#define IEC_RESET_ACTIVE(a) (!((a)&IECLINE_RESET))

#define IEC_ALL_LINES   (IECLINE_RESET|IECLINE_SRQIN|IECLINE_DATA|IECLINE_CLK|IECLINE_ATN)

#define IEC_BUS_MAX_DEVICES 4

typedef struct {
    // Each connected device pulls on its own end of the lines
    uint8_t signals;
    // // for debug purposes
    // uint8_t last_signals;
    // ...
    uint8_t id;
} iecbus_device_t;

typedef struct {
    // Up to 4 independent devices on a single bus
    iecbus_device_t devices[IEC_BUS_MAX_DEVICES];
    uint8_t usage_map;
    uint8_t lock;
    uint8_t master_tick;
} iecbus_t;

// Attach device to virtual IEC bus
iecbus_device_t* iec_connect(iecbus_t** iec_bus);
// Remove device from virtual IEC bus
void iec_disconnect(iecbus_t* iec_bus, iecbus_device_t* iec_device);
// Get total bus line status (active low)
uint8_t iec_get_signals(iecbus_t* iec_bus);
// Set a device's line status (active low)
void iec_set_signals(iecbus_t* iec_bus, iecbus_device_t* iec_device, uint8_t signals);

void iec_get_status_text(iecbus_t* iec_bus, char* dest);
void iec_get_device_status_text(iecbus_device_t* iec_device, char* dest);
void iec_debug_print_device_signals(iecbus_device_t* device, char* prefix);
void world_tick();
uint64_t get_world_tick();
void set_master_tick(iecbus_t *iec_bus);
void clear_master_tick(iecbus_t *iec_bus);

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
//#define CHIPS_IMPL
#ifdef CHIPS_IMPL

void set_master_tick(iecbus_t *iec_bus) {
    iec_bus->master_tick = 1;
}

void clear_master_tick(iecbus_t *iec_bus) {
    iec_bus->master_tick = 0;
}

iecbus_device_t* iec_connect(iecbus_t** iec_bus) {
    uint8_t i = 0;
    iecbus_device_t* bus_device = NULL;
    iecbus_t *bus = NULL;

    #ifdef IECBUS_USE_SHM
    int fd = shm_open("/iec_bus", O_CREAT | O_RDWR, 0600);
    struct stat st;
    fstat(fd, &st);
    if (st.st_size == 0) {
        ftruncate(fd, sizeof(**iec_bus));
    }
    *iec_bus = (iecbus_t*) mmap(NULL, sizeof(iecbus_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    #else
    *iec_bus = malloc(sizeof(iecbus_t));
    CHIPS_ASSERT(iec_bus);
    #endif

    bus = *iec_bus;

    const int sem = bus->lock++;

    while (sem > 0 && bus->lock > 0) {

    }

    for (i = 0; i < IEC_BUS_MAX_DEVICES && bus_device == NULL; i++) {
        if ((bus->usage_map & (1<<i)) == 0) {
            bus->usage_map |= 1<<i;
            printf("IEC device connected: Slot %d\n", i);
            bus_device = &bus->devices[i];

            // Initialize lines as inactive/high
            bus_device->signals = IEC_ALL_LINES;
            bus_device->id = i;
        }
    }

    bus->lock--;

    return bus_device;
}

void iec_disconnect(iecbus_t* iec_bus, iecbus_device_t* iec_device) {
    uint8_t i = 0;

    for (i = 0; i < IEC_BUS_MAX_DEVICES; i++) {
        if ((&iec_bus->devices[i]) == iec_device) {
            iec_bus->usage_map &= ~(1<<i);
            printf("IEC device disconnected: Slot %d\n", i);
        }
    }

    #ifdef IECBUS_USE_SHM
    munmap(iec_bus, sizeof(*iec_bus));
    #else
    free(iec_bus);
    #endif
}

uint8_t _iec_get_device_index(iecbus_t* iec_bus, iecbus_device_t* device) {
    for (int i = 0; i < IEC_BUS_MAX_DEVICES; i++) {
        if (&iec_bus->devices[i] == device) {
            return i;
        }
    }
    return IEC_BUS_MAX_DEVICES;
}

uint8_t iec_get_signals(iecbus_t* iec_bus) {
    uint8_t signals = IEC_ALL_LINES;
    for (uint i = 0; i < IEC_BUS_MAX_DEVICES; i++) {
        if (iec_bus->usage_map & (1<<i)) {
            // Active/low device signals pull down lines on the bus
            signals &= iec_bus->devices[i].signals;
        }
    }
    return signals;
}

void iec_set_signals(iecbus_t* iec_bus, iecbus_device_t* iec_device, uint8_t signals) {
    iec_device->signals = signals;
}

void iec_get_status_text(iecbus_t* iec_bus, char* dest) {
    uint8_t iec_status = iec_get_signals(iec_bus);
    dest[4] = '\0';
    if (iec_status & IECLINE_CLK) {
        dest[0] = 'c';
    } else {
        dest[0] = 'C';
    }
    if (iec_status & IECLINE_DATA) {
        dest[1] = 'd';
    } else {
        dest[1] = 'D';
    }
    if (iec_status & IECLINE_ATN) {
        dest[2] = 'a';
    } else {
        dest[2] = 'A';
    }
    if (iec_status & IECLINE_RESET) {
        dest[3] = 'r';
    } else {
        dest[3] = 'R';
    }
}

void iec_get_device_status_text(iecbus_device_t* iec_device, char* dest) {
    uint8_t iec_status = iec_device->signals;
    dest[4] = '\0';
    if (iec_status & IECLINE_CLK) {
        dest[0] = 'c'; // inactive/high
    } else {
        dest[0] = 'C'; // active/low
    }
    if (iec_status & IECLINE_DATA) {
        dest[1] = 'd';
    } else {
        dest[1] = 'D';
    }
    if (iec_status & IECLINE_ATN) {
        dest[2] = 'a';
    } else {
        dest[2] = 'A';
    }
    if (iec_status & IECLINE_RESET) {
        dest[3] = 'r';
    } else {
        dest[3] = 'R';
    }
}

void iec_debug_print_device_signals(iecbus_device_t* device, char* prefix) {
    // return;
    uint8_t signals = device->signals;
    printf("%s\t", prefix);
    if (!(signals & IECLINE_ATN)) {
        printf("ATN\t"); // active/low
    }
    if (!(signals & IECLINE_CLK)) {
        printf("CLK\t");
    }
    if (!(signals & IECLINE_DATA)) {
        printf("DATA");
    }
    printf("\n");
}

uint64_t _world_tick = 0;

void world_tick() {
    _world_tick++;
}

uint64_t get_world_tick() {
    return _world_tick;
}

#endif // CHIPS_IMPL

#endif // Z_IECBUS_H
