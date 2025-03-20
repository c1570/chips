#pragma once
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

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// #define __IEC_DEBUG

#define IECLINE_RESET (1<<0)
#define IECLINE_SRQIN (1<<1)
#define IECLINE_DATA  (1<<2)
#define IECLINE_CLK   (1<<3)
#define IECLINE_ATN   (1<<4)

#define IEC_ALL_LINES   (IECLINE_RESET|IECLINE_SRQIN|IECLINE_DATA|IECLINE_CLK|IECLINE_ATN)

#define IEC_BUS_MAX_DEVICES 4

typedef struct {
    // Used for ease of signal handling on each connected device
    uint8_t signal_invert_mask;
    // Each connected device pulls on its own end of the lines
    uint8_t signals;
    // for debug purposes
    uint8_t last_signals;
    // ...
    uint8_t id;
} iecbus_device_t;

typedef struct {
    // Up to 4 independent devices on a single bus
    iecbus_device_t devices[IEC_BUS_MAX_DEVICES];
    uint8_t usage_map;
} iecbus_t;

// Attach device to virtual IEC bus, use invert_logic if the connecting device has inverters on its bus output
iecbus_device_t* iec_connect(iecbus_t* iec_bus, bool invert_logic);
// Remove device from virtual IEC bus
void iec_disconnect(iecbus_t* iec_bus, iecbus_device_t* iec_device);
// Get total bus line status, using requesting_device for optional signal negation
uint8_t iec_get_signals(iecbus_t* iec_bus, iecbus_device_t* requesting_device);

void iec_get_status_text(iecbus_t* iec_bus, char* dest);
void iec_get_device_status_text(iecbus_device_t* iec_device, char* dest);
void iec_debug_print_device_signals(iecbus_device_t* device, char* prefix);
void world_tick();
uint64_t get_world_tick();

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL

iecbus_device_t* iec_connect(iecbus_t* iec_bus, bool invert_logic) {
    uint8_t i = 0;
    iecbus_device_t* bus_device = NULL;

    for (i = 0; i < IEC_BUS_MAX_DEVICES && bus_device == NULL; i++) {
        if ((iec_bus->usage_map & (1<<i)) == 0) {
            iec_bus->usage_map |= 1<<i;
            printf("IEC device connected: Slot %d\n", i);
            bus_device = &iec_bus->devices[i];

            // Initialize lines with the correct signal to not pull the line
            bus_device->signals = 0;
            if (invert_logic) {
                bus_device->signal_invert_mask = IEC_ALL_LINES;
            } else {
                bus_device->signal_invert_mask = 0;
            }

            bus_device->id = i;
        }
    }

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
}

uint8_t _iec_get_device_index(iecbus_t* iec_bus, iecbus_device_t* device) {
    for (int i = 0; i < IEC_BUS_MAX_DEVICES; i++) {
        if (&iec_bus->devices[i] == device) {
            return i;
        }
    }
    return IEC_BUS_MAX_DEVICES;
}

uint8_t last_iec_signals = 255;
uint8_t iec_get_signals(iecbus_t* iec_bus, iecbus_device_t* requesting_device) {
    uint8_t i = 0;
    // Initialize resulting signals with IEC logical low meaning high level voltage
    uint8_t signals = IEC_ALL_LINES;
    uint8_t device_signals = 0;
    bool changed = false;

    for (i = 0; i < IEC_BUS_MAX_DEVICES; i++) {
        if (iec_bus->usage_map & (1<<i)) {
            // Take the signals the device sets and turn them into IEC high and low voltage
            device_signals = (iec_bus->devices[i].signals ^ iec_bus->devices[i].signal_invert_mask) & IEC_ALL_LINES;
            // Let the device signals pull down lines on the bus
            signals &= device_signals;
            changed = changed || (iec_bus->devices[i].signals != iec_bus->devices[i].last_signals);
            iec_bus->devices[i].last_signals = iec_bus->devices[i].signals;
        }
    }

    if (changed) {
        printf("%ld - iecbus - bus: %c%c%c - device 0: %02X - device 8: %02X\n",
               get_world_tick(),
               (signals & IECLINE_ATN) ? 'A' : 'a',
               (signals & IECLINE_DATA) ? 'D' : 'd',
               (signals & IECLINE_CLK) ? 'C' : 'c',
               iec_bus->devices[0].signals,
               iec_bus->devices[1].signals
        );
    }

    if (requesting_device != NULL) {
        // Transform the result into the logic levels of the requesting device
        signals ^= requesting_device->signal_invert_mask;
    }

    return signals;
}

void iec_get_status_text(iecbus_t* iec_bus, char* dest) {
    uint8_t iec_status = iec_get_signals(iec_bus, NULL);
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
    uint8_t iec_status = iec_device->signals ^ iec_device->signal_invert_mask;
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

void iec_debug_print_device_signals(iecbus_device_t* device, char* prefix) {
    uint8_t signals = device->signals ^ device->signal_invert_mask;
    printf("%s\t", prefix);
    if (!(signals & IECLINE_ATN)) {
        printf("ATN\t");
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
