// Trace points: debugging aid for porting a game. A `trace <module> <address>` line in the patch file
// (recomp/patches.py) lifts rt_trace_point(c, address) in front of that instruction; this logs the guest registers
// there: every general register (x0-x7 arguments at a function entry, x0 the result after a call), the return
// address and the stack pointer. Each point logs its first TERA_TRACE_LIMIT passes (default 16), so a point inside a loop does not flood
// the log. It reads registers only; the game runs on unchanged.
#include <stdint.h>
#include <stdlib.h>

#include "cfg.h"
#include "host.h"
#include "layout.h"
#include "platform/platform.h"

#define SLOTS 1024   // distinct trace points; more than any patch file should name

static struct { uint64_t pc; uint32_t n; } seen[SLOTS];
static PlatMutex lock = PLAT_MUTEX_INIT;
static int limit = -1;

// The text at a, when a points at a NUL-terminated run of at least 3 printable ASCII bytes (up to 96 shown).
static int text_at(uint64_t a, char *out, int cap) {
    if (a < 0x10000) return 0;
    uint8_t b[97];
    if (!plat_mem_read(a, b, sizeof b - 1)) return 0;
    int n = 0;
    while (n < 96 && n < cap - 1 && b[n] >= 0x20 && b[n] < 0x7f) out[n] = (char)b[n], n++;
    out[n] = 0;
    return n >= 3 && (n == 96 || b[n] == 0);
}
static uint64_t sleep_pc;
static uint32_t sleep_ms;
static int sleep_done;

void rt_trace_point(Cpu *c, uint64_t pc) {
    plat_mutex_lock(&lock);
    if (limit < 0) {
        const char *v = cfg_get("TERA_TRACE_LIMIT");
        limit = v && *v ? atoi(v) : 16;
        const char *z = cfg_get("TERA_TRACE_SLEEP");
        if (z && *z) {
            char *end;
            uint64_t off = strtoull(z, &end, 16);
            if (*end == ':') {
                sleep_pc = MOD_MAIN_BASE + off;
                sleep_ms = (uint32_t)strtoul(end + 1, NULL, 10);
            }
        }
    }
    int do_sleep = pc == sleep_pc && !sleep_done;
    if (do_sleep) sleep_done = 1;
    uint32_t h = (uint32_t)(pc >> 2) & (SLOTS - 1), n = 0;
    for (int k = 0; k < SLOTS; k++, h = (h + 1) & (SLOTS - 1)) {
        if (!seen[h].pc) seen[h].pc = pc;
        if (seen[h].pc == pc) {
            n = ++seen[h].n;
            break;
        }
    }
    plat_mutex_unlock(&lock);
    if (do_sleep) {
        rt_log("trace: sleeping %u ms at %#llx (TERA_TRACE_SLEEP), core given up", sleep_ms, (unsigned long long)pc);
        int away = kern_core_away();
        plat_sleep_ms(sleep_ms);
        kern_core_back(&away);
        rt_log("trace: woke at %#llx", (unsigned long long)pc);
    }
    if (!n || (int)n > limit) return;
    char at[128], lr[128];
    ldr_describe(pc, at, sizeof at);
    ldr_describe(c->x[30] - 4, lr, sizeof lr);
    rt_log("trace %s #%u: x0 %llx x1 %llx x2 %llx x3 %llx x4 %llx x5 %llx x6 %llx x7 %llx x8 %llx | x19 %llx x20 %llx "
           "x21 %llx x22 %llx x23 %llx x24 %llx x25 %llx x26 %llx x27 %llx x28 %llx | lr %s sp %llx",
           at, n, (unsigned long long)c->x[0], (unsigned long long)c->x[1], (unsigned long long)c->x[2],
           (unsigned long long)c->x[3], (unsigned long long)c->x[4], (unsigned long long)c->x[5],
           (unsigned long long)c->x[6], (unsigned long long)c->x[7], (unsigned long long)c->x[8],
           (unsigned long long)c->x[19], (unsigned long long)c->x[20], (unsigned long long)c->x[21],
           (unsigned long long)c->x[22], (unsigned long long)c->x[23], (unsigned long long)c->x[24],
           (unsigned long long)c->x[25], (unsigned long long)c->x[26], (unsigned long long)c->x[27],
           (unsigned long long)c->x[28], lr, (unsigned long long)c->sp);
    rt_log("trace   x9 %llx x10 %llx x11 %llx x12 %llx x13 %llx x14 %llx x15 %llx x16 %llx x17 %llx x18 %llx",
           (unsigned long long)c->x[9], (unsigned long long)c->x[10], (unsigned long long)c->x[11],
           (unsigned long long)c->x[12], (unsigned long long)c->x[13], (unsigned long long)c->x[14],
           (unsigned long long)c->x[15], (unsigned long long)c->x[16], (unsigned long long)c->x[17],
           (unsigned long long)c->x[18]);
    static const int text_regs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28};
    for (unsigned k = 0; k < sizeof text_regs / sizeof *text_regs; k++) {
        char t[100];
        if (text_at(c->x[text_regs[k]], t, sizeof t)) rt_log("trace   x%d -> \"%s\"", text_regs[k], t);
    }
    uint64_t obj[16];
    if (c->x[19] && plat_mem_read(c->x[19], obj, sizeof obj)) {
        char vt[128];
        ldr_describe(obj[0], vt, sizeof vt);
        rt_log("trace   [x19 %llx] vtable %s | +08 %llx +10 %llx +18 %llx +20 %llx +28 %llx +30 %llx +38 %llx +40 %llx "
               "+48 %llx +50 %llx +58 %llx +60 %llx +68 %llx +70 %llx +78 %llx",
               (unsigned long long)c->x[19], vt, (unsigned long long)obj[1], (unsigned long long)obj[2],
               (unsigned long long)obj[3], (unsigned long long)obj[4], (unsigned long long)obj[5],
               (unsigned long long)obj[6], (unsigned long long)obj[7], (unsigned long long)obj[8],
               (unsigned long long)obj[9], (unsigned long long)obj[10], (unsigned long long)obj[11],
               (unsigned long long)obj[12], (unsigned long long)obj[13], (unsigned long long)obj[14],
               (unsigned long long)obj[15]);
    }
}
