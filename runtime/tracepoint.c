// Trace points: debugging aid for porting a game. A `trace <module> <address>` line in the patch file
// (recomp/patches.py) lifts rt_trace_point(c, address) in front of that instruction; this logs the guest registers
// there: the arguments x0-x7 at a function entry, the result x0 after a call, the return address and the stack
// pointer. Each point logs its first TERA_TRACE_LIMIT passes (default 16), so a point inside a loop does not flood
// the log. It reads registers only; the game runs on unchanged.
#include <stdint.h>
#include <stdlib.h>

#include "cfg.h"
#include "host.h"
#include "platform/platform.h"

#define SLOTS 1024   // distinct trace points; more than any patch file should name

static struct { uint64_t pc; uint32_t n; } seen[SLOTS];
static PlatMutex lock = PLAT_MUTEX_INIT;
static int limit = -1;

void rt_trace_point(Cpu *c, uint64_t pc) {
    plat_mutex_lock(&lock);
    if (limit < 0) {
        const char *v = cfg_get("TERA_TRACE_LIMIT");
        limit = v && *v ? atoi(v) : 16;
    }
    uint32_t h = (uint32_t)(pc >> 2) & (SLOTS - 1), n = 0;
    for (int k = 0; k < SLOTS; k++, h = (h + 1) & (SLOTS - 1)) {
        if (!seen[h].pc) seen[h].pc = pc;
        if (seen[h].pc == pc) {
            n = ++seen[h].n;
            break;
        }
    }
    plat_mutex_unlock(&lock);
    if (!n || (int)n > limit) return;
    char at[128], lr[128];
    ldr_describe(pc, at, sizeof at);
    ldr_describe(c->x[30] - 4, lr, sizeof lr);
    rt_log("trace %s #%u: x0 %llx x1 %llx x2 %llx x3 %llx x4 %llx x5 %llx x6 %llx x7 %llx | x19 %llx x20 %llx | "
           "lr %s sp %llx",
           at, n, (unsigned long long)c->x[0], (unsigned long long)c->x[1], (unsigned long long)c->x[2],
           (unsigned long long)c->x[3], (unsigned long long)c->x[4], (unsigned long long)c->x[5],
           (unsigned long long)c->x[6], (unsigned long long)c->x[7], (unsigned long long)c->x[19],
           (unsigned long long)c->x[20], lr, (unsigned long long)c->sp);
}
