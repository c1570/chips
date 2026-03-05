/*
    c64-ascii.c

    Stripped down C64 emulator running in a (xterm-256color) terminal.
    Modified from
    https://github.com/floooh/chips-test/tree/master/examples/ascii
*/
#include <stdint.h>
#include <stdbool.h>
#include <curses.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#define CHIPS_IMPL
#include "../chips/chips_common.h"
#include "../chips/m6502.h"
#include "../chips/m6526.h"
#include "../chips/m6569.h"
#include "../chips/m6581.h"
#include "../chips/beeper.h"
#include "../chips/kbd.h"
#include "../chips/mem.h"
#include "../chips/clk.h"
#include "../systems/c1530.h"
#include "../chips/m6522.h"
static bool drive_led_status = 0;
static bool drive_motor_status = 1;
static int drive_current_halftrack;
#define C1541_TRACK_CHANGED_HOOK(s,v) drive_current_halftrack=v
#define C1541_MOTOR_CHANGED_HOOK(s,v) drive_motor_status=v
#define C1541_LED_CHANGED_HOOK(s,v) drive_led_status=v
//#define C64_ENABLE_DEBUG
//#define C1541_ENABLE_DEBUG
#include "../systems/c1541.h"
#include "../systems/disass.h"
#include "../systems/c1541_debug.h"
#include "c64_system_novic.h"
#include "c64-roms.h"
#include "c1541-roms.h"

static c64_t c64;

// Logging state
static uint64_t start_cycle = 0;  // Cycle at which to start logging (0 = disabled)
static uint64_t current_cycle = 0; // Current CPU cycle counter
static bool logging_enabled = false; // Whether logging is currently enabled

// PRG injection state
static char* prg_filename = NULL; // .prg file to inject

// Forward declarations
static void log_cycle(c64_t* sys, uint64_t pins);
static bool load_prg_file(c64_t* sys, const char* filename);

// Debug callback for cycle-accurate logging
static void debug_callback(void* user_data, uint64_t pins) {
    current_cycle++;

    if (!logging_enabled && start_cycle > 0 && current_cycle >= start_cycle) {
        logging_enabled = true;
    }

    if (logging_enabled) {
        log_cycle(&c64, pins);
    }
}

// run the emulator and render-loop at 30fps
#define FRAME_USEC (33333)
// border size
#define BORDER_HORI (5)
#define BORDER_VERT (3)

// a signal handler for Ctrl-C, for proper cleanup
static int quit_requested = 0;
static void catch_sigint(int signo) {
    (void)signo;
    quit_requested = 1;
}

// conversion table from C64 font index to ASCII (the 'x' is actually the pound sign)
static char font_map[65] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[x]   !\"#$%&`()*+,-./0123456789:;<=>?";

// map C64 color numbers to xterm-256color colors
static int colors[16] = {
    16,     // black
    231,    // white
    88,     // red
    73,     // cyan
    54,     // purple
    71,     // green
    18,     // blue
    185,    // yellow
    136,    // orange
    58,     // brown
    131,    // light-red
    59,     // dark-grey
    102,    // grey
    150,    // light green
    62,     // light blue
    145,    // light grey
};

static void init_c64_colors(void) {
    start_color();
    for (int fg = 0; fg < 16; fg++) {
        for (int bg = 0; bg < 16; bg++) {
            int cp = (fg*16 + bg) + 1;
            init_pair(cp, colors[fg], colors[bg]);
        }
    }
}

void set_keybuf(char* str) {
    c64.ram[198] = strlen(str);
    for(uint i=0; i<c64.ram[198]; i++) { c64.ram[631+i]=str[i]; };
}

void update_screen(c64_t* c64) {
    int cur_color_pair = -1;
    int bg = c64->vic.gunit.bg[0] & 0xF;
    int bc = c64->vic.brd.bc & 0xF;
    for (uint32_t yy = 0; yy < 25+2*BORDER_VERT; yy++) {
        for (uint32_t xx = 0; xx < 40+2*BORDER_HORI; xx++) {
            if ((xx < BORDER_HORI) || (xx >= 40+BORDER_HORI) ||
                (yy < BORDER_VERT) || (yy >= 25+BORDER_VERT))
            {
                // border area
                int color_pair = bc+1;
                if (color_pair != cur_color_pair) {
                    attron(COLOR_PAIR(color_pair));
                    cur_color_pair = color_pair;
                }
                mvaddch(yy, xx*2, ' ');
                mvaddch(yy, xx*2+1, ' ');
            }
            else {
                // bitmap area (not border)
                int x = xx - BORDER_HORI;
                int y = yy - BORDER_VERT;

                // get color byte (only lower 4 bits wired)
                int fg = c64->color_ram[y*40+x] & 15;
                int color_pair = (fg*16+bg)+1;
                if (color_pair != cur_color_pair) {
                    attron(COLOR_PAIR(color_pair));
                    cur_color_pair = color_pair;
                }

                // get character index
                uint16_t addr = 0x0400 + y*40 + x;
                uint8_t font_code = mem_rd(&c64->mem_vic, addr);
                char chr = font_map[font_code & 63];
                // invert upper half of character set
                if (font_code > 127) {
                    attron(A_REVERSE);
                }
                // padding to get proper aspect ratio
                mvaddch(yy, xx*2, ' ');
                // character
                mvaddch(yy, xx*2+1, chr);
                // invert upper half of character set
                if (font_code > 127) {
                    attroff(A_REVERSE);
                }
            }
        }
    }
    attron(A_REVERSE);
    attron(COLOR_PAIR(231*16));
    mvaddch(25+BORDER_VERT+3, 99, drive_led_status ? 'X' : '.');
    mvaddch(25+BORDER_VERT+3, 97, drive_motor_status ? 'O' : '.');
    char str_track[10];
    sprintf(str_track, "%4.1f", ((float)drive_current_halftrack)/2.0);
    mvaddstr(25+BORDER_VERT+3, 92, str_track);
    attroff(A_REVERSE);
    refresh();
}

// Logging function for cycle-accurate debugging
static void log_cycle(c64_t* sys, uint64_t pins) {
    // Extract bus information
    const uint16_t bus_addr = M6502_GET_ADDR(pins);
    const uint8_t bus_data = M6502_GET_DATA(pins);
    const bool is_read = (pins & M6502_RW) != 0;

    // Extract CPU flags
    const int sync = (pins & M6502_SYNC) ? 1 : 0;
    const int rdy = (pins & M6502_RDY) ? 1 : 0;
    const int irq = (pins & M6502_IRQ) ? 1 : 0;
    const int nmi = (pins & M6502_NMI) ? 1 : 0;

    // Extract VIC-II timing info
    const int vic_cycle = sys->vic.rs.h_count;
    const int vic_line = sys->vic.rs.v_count;

    const uint16_t ir = sys->cpu.IR;

    // Print the log line
    printf("Cycle %lu: PC=$%04X A=$%02X X=$%02X Y=$%02X IR=%02x.%d BUS: addr=$%04X data=$%02X %c FLAGS: SYNC=%d RDY=%d IRQ=%d NMI=%d VIC: cycle=%d line=%d\n",
           (unsigned long)current_cycle,
           sys->cpu.PC,
           sys->cpu.A,
           sys->cpu.X,
           sys->cpu.Y,
           ir >> 3,
           (ir & 7) - 1,
           bus_addr,
           bus_data,
           is_read ? 'R' : 'W',
           sync,
           rdy,
           irq,
           nmi,
           vic_cycle,
           vic_line);
}

// Load a .prg file into C64 memory
static bool load_prg_file(c64_t* sys, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open .prg file: %s\n", filename);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 2) {
        fprintf(stderr, "Error: .prg file too small (must contain at least load address)\n");
        fclose(f);
        return false;
    }

    // Read load address (lo/hi format)
    uint8_t addr_lo, addr_hi;
    if (fread(&addr_lo, 1, 1, f) != 1 || fread(&addr_hi, 1, 1, f) != 1) {
        fprintf(stderr, "Error: Failed to read load address from .prg file\n");
        fclose(f);
        return false;
    }

    uint16_t load_addr = addr_lo | (addr_hi << 8);
    size_t data_size = file_size - 2;

    // Read program data
    uint8_t* buffer = malloc(data_size);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return false;
    }

    if (fread(buffer, 1, data_size, f) != data_size) {
        fprintf(stderr, "Error: Failed to read program data from .prg file\n");
        free(buffer);
        fclose(f);
        return false;
    }

    fclose(f);

    // Load into C64 memory
    if (load_addr + data_size > 0x10000) {
        fprintf(stderr, "Error: Program data exceeds C64 memory bounds\n");
        free(buffer);
        return false;
    }

    for (size_t i = 0; i < data_size; i++) {
        sys->ram[load_addr + i] = buffer[i];
    }

    printf("Injected .prg file: %s\n", filename);
    printf("  Load address: $%04X\n", load_addr);
    printf("  Data size: %zu bytes\n", data_size);

    // Set BASIC end-of-program and start-of-variables pointers
    // This allows BASIC to see the loaded program
    sys->ram[0x2D] = load_addr & 0xFF;        // VARTAB (lo)
    sys->ram[0x2E] = (load_addr >> 8) & 0xFF; // VARTAB (hi)
    sys->ram[0x2F] = (load_addr + data_size) & 0xFF;        // ARYTAB (lo)
    sys->ram[0x30] = ((load_addr + data_size) >> 8) & 0xFF; // ARYTAB (hi)
    sys->ram[0x31] = (load_addr + data_size) & 0xFF;        // STREND (lo)
    sys->ram[0x32] = ((load_addr + data_size) >> 8) & 0xFF; // STREND (hi)

    free(buffer);
    return true;
}

int main(int argc, char* argv[]) {
    const char* disk_filename = NULL;
    bool enable_curses = 1;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disk") == 0) {
            if (i + 1 < argc) {
                disk_filename = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires a filename argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prg") == 0) {
            if (i + 1 < argc) {
                prg_filename = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires a filename argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                start_cycle = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: %s requires a cycle number argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            enable_curses = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-d|--disk FILENAME] [-p|--prg FILENAME] [-s CYCLE] [-c] [-h|--help]\n", argv[0]);
            printf("  -d, --disk FILENAME  Attach G64 disk image\n");
            printf("  -p, --prg FILENAME   Inject .prg file into memory\n");
            printf("  -s CYCLE             Start logging at CPU cycle CYCLE\n");
            printf("  -c                   Disable ncurses\n");
            printf("  -h, --help           Show this help message\n");
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Usage: %s [-d|--disk FILENAME] [-p|--prg FILENAME] [-s CYCLE] [-c] [-h|--help]\n", argv[0]);
            return 1;
        }
    }

    // Setup debug callback for logging if start_cycle is specified
    static bool debug_stopped = false;
    c64_init(&c64, &(c64_desc_t){
        .roms = {
            .chars = { .ptr=dump_c64_char_bin, .size=sizeof(dump_c64_char_bin) },
            .basic = { .ptr=dump_c64_basic_bin, .size=sizeof(dump_c64_basic_bin) },
            .kernal = { .ptr=dump_c64_kernalv3_bin, .size=sizeof(dump_c64_kernalv3_bin) },
            .c1541 = {
                .c000_dfff = { .ptr=dump_1541_c000_325302_01_bin, .size=sizeof(dump_1541_c000_325302_01_bin) },
                .e000_ffff = { .ptr=dump_1541_e000_901229_06aa_bin, .size=sizeof(dump_1541_e000_901229_06aa_bin) }
            }
        },
        .c1541_enabled = 1,
        .debug = {
            .callback = {
                .func = start_cycle > 0 ? debug_callback : NULL,
                .user_data = NULL
            },
            .stopped = &debug_stopped
        }
    });

    // Attach disk image if specified
    if (disk_filename != NULL) {
        if (!c1541_attach_disk(&c64.c1541, disk_filename)) {
            fprintf(stderr, "Warning: Failed to attach disk image: %s\n", disk_filename);
        }
    }
    drive_current_halftrack = c64.c1541.half_track;

    // install a Ctrl-C signal handler
    signal(SIGINT, catch_sigint);

    // setup curses
    if (enable_curses) {
        initscr();
        init_c64_colors();
        noecho();
        curs_set(FALSE);
        cbreak();
        nodelay(stdscr, TRUE);
        keypad(stdscr, TRUE);
        attron(A_BOLD);
    }
    uint c64_ticks = 0;
    uint keysim_state = 0;

    // run the emulation/input/render loop
    while (!quit_requested) {
        // tick the emulator for 1 frame
        c64_ticks += c64_exec(&c64, FRAME_USEC);

        // Inject .prg file at cycle 200000 if specified

        #ifdef PRGDEBUG
        if(c64_ticks > 150000 && keysim_state == 0) {
        #else
        if (prg_filename && c64_ticks > 150000) {
        #endif
            load_prg_file(&c64, prg_filename);
            prg_filename = NULL;
            #ifdef PRGDEBUG
            set_keybuf("L\x6f\"*\",8,1\r");
            #else
            set_keybuf("RUN\r");
            #endif
            keysim_state++;
        }

        // keyboard input
        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case 10:  ch = 0x0D; break; // ENTER
                case 263:
                case 127: ch = 0x01; break; // BACKSPACE
                case 27:  ch = 0x03; break; // ESCAPE
                case 260: ch = 0x08; break; // LEFT
                case 261: ch = 0x09; break; // RIGHT
                case 259: ch = 0x0B; break; // UP
                case 258: ch = 0x0A; break; // DOWN
                case 265: ch = C64_KEY_F1; break;
                case 267: ch = C64_KEY_F3; break;
                case 269: ch = C64_KEY_F5; break;
                case 271: ch = C64_KEY_F7; break;
            }
            if (ch > 32) {
                if (islower(ch)) {
                    ch = toupper(ch);
                }
                else if (isupper(ch)) {
                    ch = tolower(ch);
                }
            }
            if (ch < 256) {
                c64_key_down(&c64, ch);
                c64_key_up(&c64, ch);
            }
        }
        if (enable_curses) {
            update_screen(&c64);
        }

        // pause until next frame
        //usleep(FRAME_USEC);
        #ifdef PRGDEBUG
        if(c64_ticks>9000000) quit_requested = 1;
        #endif
    }
    if (enable_curses) {
        endwin();
    }
    printf("Stopped at tick %d\n", c64_ticks);
    return 0;
}
