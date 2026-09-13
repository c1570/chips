/*
    w65c816_test.c

    Test runner for chips/w65c816.h against the SingleStepTests_65816
    test suite (https://github.com/SingleStepTests/65816).

    Each test provides the full CPU state before/after execution of a
    single instruction, plus an exact cycle-by-cycle bus trace. This
    runner replays each test cycle-exactly:

        - loads the initial CPU state
        - sets up the "opcode fetch" pin state (this is trace cycle 0)
        - ticks the CPU and compares each bus cycle against the trace
        - verifies the final CPU state and memory content

    Usage:

        w65c816_test [--data DIR] [--mode n|e|both] [--opcodes SPEC]
                     [--first N] [--stop-on-fail] [--max-fail N]
                     [--quiet] [--verbose]

        --data DIR      path to the SingleStepTests_65816 'v1' directory
        --mode          which test files to run: n (native), e (emulation),
                        both (default)
        --opcodes SPEC  comma-separated opcode list/ranges, e.g. "18,1d,20-2f"
                        or "all" (default)
        --first N       only run the first N tests per file (0 = all)
        --stop-on-fail  stop at the first failing test
        --max-fail N    stop after N failing tests (default: no limit)
        --quiet         don't print per-file summaries
        --verbose       print details for every failing cycle
*/
#define CHIPS_IMPL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include "../chips/w65c816.h"

/* 16 MBytes flat memory (tests assume full 24-bit address space) */
#define MEMSIZE (1<<24)
static uint8_t* mem;
/* addresses touched by the current test (for quick reset) */
static uint32_t* touched;
static int num_touched;

/*--- minimal JSON parser, just enough for the test file format ---*/
typedef struct {
    const char* p;
    const char* end;
    bool err;
} jp_t;

static void jws(jp_t* j) {
    while ((j->p < j->end) && ((*j->p==' ')||(*j->p=='\t')||(*j->p=='\n')||(*j->p=='\r'))) {
        j->p++;
    }
}

static bool jchar(jp_t* j, char c) {
    jws(j);
    if ((j->p < j->end) && (*j->p == c)) {
        j->p++;
        return true;
    }
    else {
        return false;
    }
}

static bool jpeek(jp_t* j, char c) {
    jws(j);
    return (j->p < j->end) && (*j->p == c);
}

static bool jstring(jp_t* j, char* dst, size_t cap) {
    jws(j);
    if ((j->p >= j->end) || (*j->p != '"')) {
        return false;
    }
    j->p++;
    size_t i = 0;
    while ((j->p < j->end) && (*j->p != '"')) {
        char c = *j->p++;
        if ((c == '\\') && (j->p < j->end)) {
            c = *j->p++;
        }
        if (i < (cap-1)) {
            dst[i++] = c;
        }
    }
    if ((j->p >= j->end) || (*j->p != '"')) {
        return false;
    }
    j->p++;
    dst[i] = 0;
    return true;
}

static bool jnumber(jp_t* j, int64_t* out) {
    jws(j);
    const char* s = j->p;
    if ((j->p < j->end) && (*j->p == '-')) {
        j->p++;
    }
    bool valid = false;
    while ((j->p < j->end) && (*j->p >= '0') && (*j->p <= '9')) {
        j->p++;
        valid = true;
    }
    if (!valid) {
        return false;
    }
    *out = strtoll(s, NULL, 10);
    return true;
}

/* skip any JSON value (object, array, string, number, true, false, null) */
static bool jskip(jp_t* j) {
    if (j->err) return false;
    jws(j);
    if (j->p >= j->end) return false;
    char c = *j->p;
    if (c == '"') {
        char buf[8];
        return jstring(j, buf, sizeof(buf));
    }
    else if ((c == '-') || ((c >= '0') && (c <= '9'))) {
        int64_t v;
        return jnumber(j, &v);
    }
    else if (c == '{') {
        j->p++;
        if (jchar(j, '}')) return true;
        for (;;) {
            char key[64];
            if (!jstring(j, key, sizeof(key)) || !jchar(j, ':') || !jskip(j)) {
                return false;
            }
            if (jchar(j, ',')) continue;
            return jchar(j, '}');
        }
    }
    else if (c == '[') {
        j->p++;
        if (jchar(j, ']')) return true;
        for (;;) {
            if (!jskip(j)) return false;
            if (jchar(j, ',')) continue;
            return jchar(j, ']');
        }
    }
    else if (0 == strncmp(j->p, "true", (size_t)(j->end - j->p) < 4 ? (size_t)(j->end - j->p) : 4)) {
        j->p += 4; return true;
    }
    else if (0 == strncmp(j->p, "false", (size_t)(j->end - j->p) < 5 ? (size_t)(j->end - j->p) : 5)) {
        j->p += 5; return true;
    }
    else if (0 == strncmp(j->p, "null", (size_t)(j->end - j->p) < 4 ? (size_t)(j->end - j->p) : 4)) {
        j->p += 4; return true;
    }
    return false;
}

/*--- test data structures ---*/
static bool jnum_or_null(jp_t* j, int64_t* out);

#define MAX_RAM 256
#define MAX_CYCLES 256
typedef struct {
    int64_t pc, s, p, a, x, y, dbr, d, pbr, e;
    int64_t ram[MAX_RAM][2];
    int nram;
} state_t;

typedef struct {
    bool has_addr, has_val;
    int64_t addr, val;
    char flags[16];
} cyc_t;

typedef struct {
    char name[128];
    state_t initial, final;
    cyc_t cycles[MAX_CYCLES];
    int ncycles;
} test_t;

static bool jstate(jp_t* j, state_t* st) {
    memset(st, 0, sizeof(*st));
    if (!jchar(j, '{')) return false;
    for (;;) {
        if (jpeek(j, '}')) { j->p++; return true; }
        char key[32];
        if (!jstring(j, key, sizeof(key)) || !jchar(j, ':')) return false;
        if (0 == strcmp(key, "ram")) {
            if (!jchar(j, '[')) return false;
            while (!jpeek(j, ']')) {
                if (!jchar(j, '[')) return false;
                int64_t a = -1, v = -1;
                if (!jnum_or_null(j, &a)) return false;
                if (!jchar(j, ',')) return false;
                if (!jnum_or_null(j, &v)) return false;
                if (!jchar(j, ']')) return false;
                if (st->nram < MAX_RAM) {
                    st->ram[st->nram][0] = a;
                    st->ram[st->nram][1] = v;
                    st->nram++;
                }
                if (jchar(j, ',')) continue;
                break;
            }
            if (!jchar(j, ']')) return false;
        }
        else {
            int64_t* dst = 0;
            if      (0 == strcmp(key, "pc"))  dst = &st->pc;
            else if (0 == strcmp(key, "s"))   dst = &st->s;
            else if (0 == strcmp(key, "p"))   dst = &st->p;
            else if (0 == strcmp(key, "a"))   dst = &st->a;
            else if (0 == strcmp(key, "x"))   dst = &st->x;
            else if (0 == strcmp(key, "y"))   dst = &st->y;
            else if (0 == strcmp(key, "dbr")) dst = &st->dbr;
            else if (0 == strcmp(key, "d"))   dst = &st->d;
            else if (0 == strcmp(key, "pbr")) dst = &st->pbr;
            else if (0 == strcmp(key, "e"))   dst = &st->e;
            int64_t v = 0;
            if (!jnum_or_null(j, &v)) return false;
            if (v < 0) v = 0;
            if (dst) *dst = v;
        }
        if (jchar(j, ',')) continue;
        return jchar(j, '}');
    }
}

static bool jcycle(jp_t* j, cyc_t* cy) {
    memset(cy, 0, sizeof(*cy));
    if (!jchar(j, '[')) return false;
    if (jpeek(j, 'n')) {
        if (!jskip(j)) return false;
    }
    else {
        if (!jnumber(j, &cy->addr)) return false;
        cy->has_addr = true;
    }
    if (!jchar(j, ',')) return false;
    if (jpeek(j, 'n')) {
        if (!jskip(j)) return false;
    }
    else {
        if (!jnumber(j, &cy->val)) return false;
        cy->has_val = true;
    }
    if (!jchar(j, ',')) return false;
    if (!jstring(j, cy->flags, sizeof(cy->flags))) return false;
    return jchar(j, ']');
}

static bool jtest(jp_t* j, test_t* t) {
    memset(t, 0, sizeof(*t));
    if (!jchar(j, '{')) return false;
    for (;;) {
        if (jpeek(j, '}')) { j->p++; return true; }
        char key[32];
        if (!jstring(j, key, sizeof(key)) || !jchar(j, ':')) return false;
        if (0 == strcmp(key, "name")) {
            if (!jstring(j, t->name, sizeof(t->name))) return false;
        }
        else if (0 == strcmp(key, "initial")) {
            if (!jstate(j, &t->initial)) return false;
        }
        else if (0 == strcmp(key, "final")) {
            if (!jstate(j, &t->final)) return false;
        }
        else if (0 == strcmp(key, "cycles")) {
            if (!jchar(j, '[')) return false;
            while (!jpeek(j, ']')) {
                if (t->ncycles < MAX_CYCLES) {
                    if (!jcycle(j, &t->cycles[t->ncycles++])) return false;
                }
                else {
                    if (!jskip(j)) return false;
                }
                if (jchar(j, ',')) continue;
                break;
            }
            if (!jchar(j, ']')) return false;
        }
        else {
            if (!jskip(j)) return false;
        }
        if (jchar(j, ',')) continue;
        return jchar(j, '}');
    }
}

/*--- test runner ---*/
typedef struct {
    const char* datadir;
    int mode_n, mode_e;         /* which file types to run */
    bool opcodes[256];
    int first;                  /* max tests per file, 0=all */
    bool stop_on_fail;
    int max_fail;
    bool quiet;
    bool verbose;
    /* stats */
    int files_run;
    int tests_run;
    int tests_failed;
    int anom_s, anom_x, anom_y;   /* initial-state anomalies in e-mode files */
    bool cur_failed;
} args_t;

static args_t args;

/* parse a number or null (null yields -1 and has_val=false semantics via out value) */
static bool jnum_or_null(jp_t* j, int64_t* out) {
    *out = -1;
    if (jpeek(j, 'n')) {
        return jskip(j);
    }
    return jnumber(j, out);
}

/* build expected pin bits from a trace flags string "dpvremxl" */
static uint64_t flags_to_pins(const char* s) {
    uint64_t m = 0;
    for (const char* c = s; *c; c++) {
        switch (*c) {
            case 'd': m |= W65C816_VDA; break;
            case 'p': m |= W65C816_VPA; break;
            case 'v': m |= W65C816_VPB; break;
            case 'r': m |= W65C816_RW; break;
            case 'e': m |= W65C816_E; break;
            case 'm': m |= W65C816_MXM; break;
            case 'x': m |= W65C816_MXX; break;
            case 'l': m |= W65C816_MLB; break;
            default: break;
        }
    }
    return m;
}

/* pins to flags string for error messages */
static void pins_to_flags(uint64_t pins, char* out, size_t cap) {
    const char* names = "dpvremxl";
    const uint64_t bits[] = { W65C816_VDA, W65C816_VPA, W65C816_VPB, W65C816_RW,
                              W65C816_E, W65C816_MXM, W65C816_MXX, W65C816_MLB };
    size_t i = 0;
    for (int k = 0; k < 8; k++) {
        if (i < (cap-1)) {
            out[i++] = (pins & bits[k]) ? names[k] : '-';
        }
    }
    out[i] = 0;
}

/* service a bus cycle: fill data for reads, commit writes */
static uint64_t service_bus(uint64_t pins, bool commit) {
    uint32_t addr = W65C816_GET_ADDR(pins);
    bool vpin = 0 != (pins & (W65C816_VPA|W65C816_VDA|W65C816_VPB));
    if (pins & W65C816_RW) {
        /* a read: floating bus without valid-address pins */
        uint8_t v = vpin ? mem[addr & (MEMSIZE-1)] : 0xFF;
        W65C816_SET_DATA(pins, v);
    }
    else if (commit && vpin) {
        uint8_t v = W65C816_GET_DATA(pins);
        addr &= (MEMSIZE-1);
        if (mem[addr] != v) {
            mem[addr] = v;
            if (num_touched < MEMSIZE) {
                touched[num_touched++] = addr;
            }
        }
    }
    return pins;
}

static void reset_test_state(w65c816_t* c, const state_t* st) {
    w65c816_set_pc(c, (uint16_t)st->pc);
    w65c816_set_s(c, (uint16_t)st->s);
    w65c816_set_p(c, (uint8_t)st->p);
    w65c816_set_c(c, (uint16_t)st->a);
    w65c816_set_x(c, (uint16_t)st->x);
    w65c816_set_y(c, (uint16_t)st->y);
    w65c816_set_dbr(c, (uint8_t)st->dbr);
    w65c816_set_d(c, (uint16_t)st->d);
    w65c816_set_pbr(c, (uint8_t)st->pbr);
    w65c816_set_e(c, st->e != 0);
    if (c->E) {
        /* hardware forces M/X to 1, zeroes the index high bytes and
           forces the stack pointer to page 1 when in emulation mode */
        c->P |= W65C816_MF | W65C816_XF;
        c->X &= 0xFF;
        c->Y &= 0xFF;
        c->S = (uint16_t) (0x0100 | (c->S & 0xFF));
        if ((st->s & 0xFF00) != 0x0100) args.anom_s++;
        if (st->x > 0xFF) args.anom_x++;
        if (st->y > 0xFF) args.anom_y++;
    }
    c->IR = 0;
    c->AD = c->TA = c->TD = c->AA = c->RR = 0;
    c->TB = 0;
    c->CRS = 0;
    c->MC = 0;
    c->halted = 0;
    c->irq_pip = c->nmi_pip = c->abrt_pip = 0;
    c->brk_flags = 0;
}

static void report_fail(const test_t* t, const char* what, const char* fmt, ...) {
    args.tests_failed++;
    args.cur_failed = true;
    printf("FAIL: %s\n", t->name);
    printf("  %s: ", what);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* compare one traced bus cycle against the CPU pins, returns false on mismatch */
static bool cmp_cycle(const test_t* t, int idx, uint64_t pins, const cyc_t* cy) {
    char exp_flags[16], act_flags[16];
    pins_to_flags(flags_to_pins(cy->flags), exp_flags, sizeof(exp_flags));
    pins_to_flags(pins, act_flags, sizeof(act_flags));
    /* control-pin comparison (SYNC is ignored, not part of the trace) */
    uint64_t exp = flags_to_pins(cy->flags);
    uint64_t act = pins & (W65C816_VDA|W65C816_VPA|W65C816_VPB|W65C816_RW|W65C816_E|W65C816_MXM|W65C816_MXX|W65C816_MLB);
    if (exp != act) {
        report_fail(t, "cycle", " #%d: flags '%s' expected, got '%s'", idx, exp_flags, act_flags);
        return false;
    }
    if (cy->has_addr) {
        uint32_t exp_addr = (uint32_t) cy->addr;
        uint32_t act_addr = W65C816_GET_ADDR(pins);
        if (exp_addr != act_addr) {
            report_fail(t, "cycle", " #%d: addr %06X expected, got %06X (%s)", idx, exp_addr, act_addr, cy->flags);
            return false;
        }
    }
    if (cy->has_val) {
        uint8_t exp_val = (uint8_t) cy->val;
        uint8_t act_val = W65C816_GET_DATA(pins);
        if (exp_val != act_val) {
            report_fail(t, "cycle", " #%d: data %02X expected, got %02X at %06X (%s)",
                        idx, exp_val, act_val, W65C816_GET_ADDR(pins), cy->flags);
            return false;
        }
    }
    return true;
}

static bool cmp_state(const test_t* t, const state_t* st, const w65c816_t* c) {
    bool ok = true;
    struct { const char* name; int64_t exp; int64_t act; } regs[] = {
        {"pc", st->pc, c->PC},
        {"s",  st->s,  c->S},
        {"p",  st->p,  c->P},
        {"a",  st->a,  c->C},
        {"x",  st->x,  c->X},
        {"y",  st->y,  c->Y},
        {"dbr",st->dbr,c->DBR},
        {"d",  st->d,  c->D},
        {"pbr",st->pbr,c->PBR},
        {"e",  st->e,  c->E},
    };
    for (size_t i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
        if (regs[i].exp != regs[i].act) {
            report_fail(t, "final", "%s: %llX expected, got %llX", regs[i].name,
                        (unsigned long long)regs[i].exp, (unsigned long long)regs[i].act);
            ok = false;
        }
    }
    return ok;
}

static bool run_test(const test_t* t, w65c816_t* c) {
    args.cur_failed = false;
    /* reset memory touched by previous test */
    for (int i = 0; i < num_touched; i++) {
        mem[touched[i]] = 0;
    }
    num_touched = 0;
    /* load initial ram */
    for (int i = 0; i < t->initial.nram; i++) {
        uint32_t a = (uint32_t) t->initial.ram[i][0] & (MEMSIZE-1);
        uint8_t v = (uint8_t) t->initial.ram[i][1];
        if (mem[a] != v) {
            mem[a] = v;
            touched[num_touched++] = a;
        }
    }
    /* load cpu state */
    reset_test_state(c, &t->initial);
    /* set up the opcode-fetch cycle (this is trace cycle 0) */
    uint64_t pins = W65C816_RW | W65C816_SYNC | W65C816_VPA | W65C816_VDA;
    if (c->E) pins |= W65C816_E;
    if (c->P & W65C816_MF) pins |= W65C816_MXM;
    if (c->P & W65C816_XF) pins |= W65C816_MXX;
    W65C816_SET_ADDR(pins, ((uint32_t)c->PBR<<16) | c->PC);
    pins = service_bus(pins, true);
    /* compare cycle 0 */
    if (t->ncycles > 0) {
        if (!cmp_cycle(t, 0, pins, &t->cycles[0])) {
            return false;
        }
    }
    /* run and compare the remaining traced cycles */
    for (int i = 1; i < t->ncycles; i++) {
        const cyc_t* cy = &t->cycles[i];
        if (!cy->has_addr && (0 == strcmp(cy->flags, "--------"))) {
            /* halt sentinel (WAI/STP): verify the CPU stopped driving the bus */
            for (int k = 0; k < 8; k++) {
                pins = w65c816_tick(c, pins);
                pins = service_bus(pins, true);
                if (pins & (W65C816_VPA|W65C816_VDA|W65C816_VPB)) {
                    report_fail(t, "halt", "bus activity after halt sentinel");
                    return false;
                }
            }
            break;
        }
        pins = w65c816_tick(c, pins);
        pins = service_bus(pins, true);
        if (!cmp_cycle(t, i, pins, cy)) {
            return false;
        }
    }
    /* if the trace was truncated at 100 cycles (block moves), run the
       instruction to completion before verifying the final state
    */
    if ((t->ncycles == 100) && !args.cur_failed) {
        int guard = 0;
        while (!(pins & W65C816_SYNC)) {
            pins = service_bus(pins, true);
            pins = w65c816_tick(c, pins);
            if (++guard > 400000) {
                report_fail(t, "truncated", "instruction did not complete within 400k cycles");
                return false;
            }
        }
        pins = service_bus(pins, true);
    }
    /* the instruction's last microstep is the (untraced) fetch of the next
       opcode: execute it so the CPU ends up in the 'prefetched' state with
       all side effects applied, then verify the final state
    */
    if (!args.cur_failed && (t->ncycles != 100) && (t->ncycles > 0) &&
        (0 != strcmp(t->cycles[t->ncycles-1].flags, "--------")))
    {
        pins = w65c816_tick(c, pins);
        pins = service_bus(pins, true);
    }
    /* verify final cpu state */
    if (!cmp_state(t, &t->final, c)) {
        return false;
    }
    /* verify final memory content */
    for (int i = 0; i < t->final.nram; i++) {
        uint32_t a = (uint32_t) t->final.ram[i][0] & (MEMSIZE-1);
        uint8_t v = (uint8_t) t->final.ram[i][1];
        if (mem[a] != v) {
            report_fail(t, "final", "mem[%06X]: %02X expected, got %02X", a, v, mem[a]);
            return false;
        }
    }
    return true;
}

/*--- file loading and command line handling ---*/
static char* slurp_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t)sz+1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    buf[sz] = 0;
    *size = (size_t)sz;
    return buf;
}

static int run_file(const char* path, const char* opcode_name, const char* mode, w65c816_t* c) {
    size_t size;
    char* buf = slurp_file(path, &size);
    if (!buf) {
        printf("ERROR: cannot open '%s'\n", path);
        return -1;
    }
    jp_t j = { buf, buf+size, false };
    if (!jchar(&j, '[')) {
        printf("ERROR: '%s' is not a JSON array\n", path);
        free(buf);
        return -1;
    }
    int nrun = 0, nfail = 0;
    test_t t;
    while (!jpeek(&j, ']')) {
        if (!jtest(&j, &t)) {
            printf("ERROR: JSON parse error in '%s' after %d tests\n", path, nrun);
            free(buf);
            return -1;
        }
        if ((args.first > 0) && (nrun >= args.first)) {
            break;
        }
        args.tests_run++;
        nrun++;
        if (!run_test(&t, c)) {
            nfail++;
            if (args.stop_on_fail) {
                free(buf);
                args.files_run++;
                return -1;
            }
            if ((args.max_fail > 0) && (args.tests_failed >= args.max_fail)) {
                free(buf);
                args.files_run++;
                return -1;
            }
        }
        if (jchar(&j, ',')) continue;
        break;
    }
    args.files_run++;
    if (!args.quiet) {
        printf("%s.%s: %d tests, %d failed\n", opcode_name, mode, nrun, nfail);
    }
    free(buf);
    return nfail;
}

static void usage(void) {
    printf("w65c816_test -- test runner for chips/w65c816.h\n\n");
    printf("    --data DIR       path to SingleStepTests_65816/v1 (default: SingleStepTests_65816/v1)\n");
    printf("    --mode n|e|both  test native, emulation or both (default: both)\n");
    printf("    --opcodes SPEC   comma-separated opcodes/ranges, e.g. 18,1d,20-2f, or 'all'\n");
    printf("    --first N        run at most N tests per file (0 = all)\n");
    printf("    --stop-on-fail   stop at first failing test\n");
    printf("    --max-fail N     stop after N failed tests\n");
    printf("    --quiet          no per-file summary\n");
}

static void set_all_opcodes(bool val) {
    for (int i = 0; i < 256; i++) {
        args.opcodes[i] = val;
    }
}

static bool parse_opcodes(const char* s) {
    set_all_opcodes(false);
    while (*s) {
        int a = (int) strtol(s, (char**)&s, 16);
        int b = a;
        if (*s == '-') {
            s++;
            b = (int) strtol(s, (char**)&s, 16);
        }
        if ((a < 0) || (a > 255) || (b < a) || (b > 255)) {
            return false;
        }
        for (int i = a; i <= b; i++) {
            args.opcodes[i] = true;
        }
        if (*s == ',') {
            s++;
        }
        else if (*s) {
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    args.datadir = "SingleStepTests_65816/v1";
    args.mode_n = args.mode_e = true;
    set_all_opcodes(true);
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (0 == strcmp(a, "--data") && (i+1 < argc)) {
            args.datadir = argv[++i];
        }
        else if (0 == strcmp(a, "--mode") && (i+1 < argc)) {
            const char* m = argv[++i];
            if (0 == strcmp(m, "n")) { args.mode_n = true; args.mode_e = false; }
            else if (0 == strcmp(m, "e")) { args.mode_n = false; args.mode_e = true; }
            else if (0 == strcmp(m, "both")) { args.mode_n = args.mode_e = true; }
            else { printf("ERROR: invalid --mode '%s'\n", m); return 10; }
        }
        else if (0 == strcmp(a, "--opcodes") && (i+1 < argc)) {
            if (!parse_opcodes(argv[++i])) {
                printf("ERROR: invalid --opcodes spec\n");
                return 10;
            }
        }
        else if (0 == strcmp(a, "--first") && (i+1 < argc)) {
            args.first = atoi(argv[++i]);
        }
        else if (0 == strcmp(a, "--stop-on-fail")) {
            args.stop_on_fail = true;
        }
        else if (0 == strcmp(a, "--max-fail") && (i+1 < argc)) {
            args.max_fail = atoi(argv[++i]);
        }
        else if (0 == strcmp(a, "--quiet")) {
            args.quiet = true;
        }
        else if ((0 == strcmp(a, "--help")) || (0 == strcmp(a, "-h"))) {
            usage();
            return 0;
        }
        else {
            printf("ERROR: unknown arg '%s'\n", a);
            usage();
            return 10;
        }
    }
    mem = (uint8_t*) malloc(MEMSIZE);
    memset(mem, 0, MEMSIZE);
    touched = (uint32_t*) malloc(sizeof(uint32_t)*MEMSIZE);
    num_touched = 0;
    w65c816_t cpu;
    w65c816_init(&cpu, &(w65c816_desc_t){ .abort_disabled = false });
    const char* modes[2] = { "n", "e" };
    bool domodes[2] = { args.mode_n != 0, args.mode_e != 0 };
    for (int op = 0; op < 256; op++) {
        if (!args.opcodes[op]) continue;
        for (int mi = 0; mi < 2; mi++) {
            if (!domodes[mi]) continue;
            char path[512], opname[8];
            snprintf(opname, sizeof(opname), "%02x", op);
            snprintf(path, sizeof(path), "%s/%s.%s.json", args.datadir, opname, modes[mi]);
            if (run_file(path, opname, modes[mi], &cpu) < 0) {
                if (args.stop_on_fail) {
                    printf("STOPPED on failure (--stop-on-fail)\n");
                    free(mem); free(touched);
                    return 1;
                }
                if ((args.max_fail > 0) && (args.tests_failed >= args.max_fail)) {
                    printf("STOPPED after %d failed tests (--max-fail %d)\n", args.tests_failed, args.max_fail);
                    free(mem); free(touched);
                    return 1;
                }
            }
        }
    }
    printf("\n%d files, %d tests, %d failed\n", args.files_run, args.tests_run, args.tests_failed);
    if (args.anom_s || args.anom_x || args.anom_y) {
        printf("(state anomalies in e-mode files: s-high=%d x-high=%d y-high=%d)\n",
               args.anom_s, args.anom_x, args.anom_y);
    }
    free(mem);
    free(touched);
    return args.tests_failed ? 1 : 0;
}
