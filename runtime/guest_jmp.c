// Guest setjmp / longjmp (Scarlet/Violet's main imports setjmp, _setjmp, longjmp and _longjmp; TotK imports none).
//
// A guest setjmp saves a point inside a guest function that a later longjmp, from deeper in the call chain, returns
// to. In lifted code that point lies inside the generated C function, so recomp/gen.py lifts every `bl` to setjmp as
//     if (plat_jmp_save(rt_setjmp(c))) { reload the registers held in C locals } else { x0 = 0 }
// in the caller's own C frame. rt_setjmp records the guest registers longjmp has to bring back (x19-x30, sp, d8-d15
// as whole vector registers) under the guest jmp_buf's address, with the thread's host-call depth, and hands out the
// host jump buffer. longjmp finds that record, restores the guest registers and the depth, sets x0 to its value (1 for
// 0) and jumps with plat_jmp_go, which unwinds nothing, back into the lifted function, which goes on after the bl.
// The guest's own jmp_buf is never read: the record is keyed by its address.
//
// Records are per host thread (a guest thread is a host thread; fibers run on their own host stacks and keep their own
// records with the thread-local table). A jmp_buf saved again replaces its record; when the table is full the least
// recently saved record is reused, as a longjmp to a frame that returned would be undefined anyway.
#include <stdint.h>
#include <string.h>

#include "host.h"
#include "platform/platform.h"

#define RECORDS 64

typedef struct {
    uint64_t buf;            // guest jmp_buf address, 0 = free
    uint64_t seq;
    PlatJmp host;
    uint64_t x[12];          // x19..x30
    uint64_t sp;
    __uint128_t v[8];        // v8..v15 (AAPCS64 keeps d8-d15)
    int depth;               // host-call depth at setjmp (kern_host_track)
} GuestJmp;

static _Thread_local GuestJmp records[RECORDS];
static _Thread_local uint64_t record_seq;

static GuestJmp *find(uint64_t buf) {
    for (int i = 0; i < RECORDS; i++)
        if (records[i].buf == buf) return &records[i];
    return NULL;
}

PlatJmp *rt_setjmp(Cpu *c) {
    uint64_t buf = c->x[0];
    GuestJmp *r = find(buf);
    if (!r) {
        r = &records[0];
        for (int i = 1; i < RECORDS; i++)
            if (records[i].seq < r->seq) r = &records[i];
    }
    r->buf = buf;
    r->seq = ++record_seq;
    memcpy(r->x, &c->x[19], sizeof r->x);
    r->sp = c->sp;
    memcpy(r->v, &c->v[8], sizeof r->v);
    r->depth = kern_host_track && c->thread ? *kern_host_depth(c->thread) : 0;
    return &r->host;
}

// void longjmp(jmp_buf, int value) / _longjmp
static void ov_longjmp(Cpu *c) {
    uint64_t buf = c->x[0];
    int value = (int)c->x[1];
    GuestJmp *r = find(buf);
    if (!r) {
        char lr[128];
        rt_fatal("longjmp to jmp_buf %#llx from %s: no setjmp of it recorded on this thread", (unsigned long long)buf,
                 ldr_describe(c->x[30] - 4, lr, sizeof lr));
    }
    memcpy(&c->x[19], r->x, sizeof r->x);
    c->sp = r->sp;
    memcpy(&c->v[8], r->v, sizeof r->v);
    c->x[0] = (uint64_t)(uint32_t)(value ? value : 1);
    if (kern_host_track && c->thread) {
        int *d = kern_host_depth(c->thread);
        __atomic_store_n(d, r->depth, __ATOMIC_RELAXED);
    }
    plat_jmp_go(&r->host);
}

// setjmp reached other than by a lifted `bl` (a call through a pointer): the saved point would be this host function's
// frame, which returns at once, so it cannot work. Stop with the caller named instead.
static void ov_setjmp_indirect(Cpu *c) {
    char lr[128];
    rt_fatal("setjmp called through a pointer from %s: only direct calls are supported (recomp/gen.py)",
             ldr_describe(c->x[30] - 4, lr, sizeof lr));
}

HOST_OVERRIDE("longjmp", ov_longjmp);
HOST_OVERRIDE("_longjmp", ov_longjmp);
HOST_OVERRIDE("setjmp", ov_setjmp_indirect);
HOST_OVERRIDE("_setjmp", ov_setjmp_indirect);
