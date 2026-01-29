#define Z_IECBUS_H

#define IECLINE_DATA  (1<<0)
#define IECLINE_CLK   (1<<1)
#define IECLINE_ATN   (1<<2)
#define IECLINE_SRQIN (1<<3)
#define IECLINE_RESET (1<<4)
#define IECLINE_ATNA  (1<<5)

#define IEC_DATA_ACTIVE(a) (!((a)&IECLINE_DATA))
#define IEC_CLK_ACTIVE(a) (!((a)&IECLINE_CLK))
#define IEC_ATN_ACTIVE(a) (!((a)&IECLINE_ATN))
#define IEC_SRQIN_ACTIVE(a) (!((a)&IECLINE_SRQIN))
#define IEC_RESET_ACTIVE(a) (!((a)&IECLINE_RESET))
#define IEC_ATNA_ACTIVE(a) (!!((a)&IECLINE_ATNA))

#define IEC_ALL_LINES   (IECLINE_ATNA|IECLINE_RESET|IECLINE_SRQIN|IECLINE_DATA|IECLINE_CLK|IECLINE_ATN)

typedef struct {
} iecbus_device_t;

typedef struct {
} iecbus_t;

static uint iecbus_host_signals = 0;
static uint iecbus_drive_signals = 0;

iecbus_device_t* iec_connect(iecbus_t** iec_bus, bool have_atna_logic) {};
void iec_disconnect(iecbus_t* iec_bus, iecbus_device_t* iec_device) {};

uint8_t iec_get_signals(iecbus_t* iec_bus) {
    uint signals = iecbus_host_signals & iecbus_drive_signals;
    if (IEC_ATN_ACTIVE(signals) ^ IEC_ATNA_ACTIVE(iecbus_drive_signals)) {
        signals &= ~IECLINE_DATA;
    }
    return signals;
};

void iec_set_signals(iecbus_t* iec_bus, iecbus_device_t* iec_device, uint8_t signals) {
    iecbus_drive_signals = signals;
};

void iec_set_from_host_signals(uint8_t signals) {
    iecbus_host_signals = signals;
}

uint8_t iec_get_drive_out_signals() {
    if (IEC_ATN_ACTIVE(iecbus_host_signals & iecbus_drive_signals) ^ IEC_ATNA_ACTIVE(iecbus_drive_signals)) {
        return iecbus_drive_signals & ~IECLINE_DATA;
    }
    return iecbus_drive_signals;
}

/*
void iec_get_status_text(iecbus_t* iec_bus, char* dest);
void iec_get_device_status_text(iecbus_device_t* iec_device, char* dest);
void iec_debug_print_device_signals(iecbus_device_t* device, char* prefix);
void world_tick();
uint64_t get_world_tick();
void set_master_tick(iecbus_t *iec_bus);
void clear_master_tick(iecbus_t *iec_bus);
*/
