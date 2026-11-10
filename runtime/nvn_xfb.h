// Transform feedback layout in the 0x880-byte control section of an NVN vertex shader (GLSLC output; worked out from
// Scarlet's programs, docs/phase3-boot.md):
//   +0x0dc + 0x80 b + i  u8   output slot of entry i of buffer b: 0x20 + n is generic output vector n
//   +0x2dc + 0x80 b + i  u8   first component of that entry (0 in every program seen)
//   +0x4dc + 0x80 b + i  u8   component count of that entry (0 ends the list)
//   +0x6dc + 2 b         u16  stride of buffer b in bytes
//   +0x6e8 + b           u8   buffer b captured
//   +0x6ec               u8   buffer count; +0x6ed: transform feedback on
// nvn_xfb_layout turns that into the Maxwell register form Ryujinx's translator takes: per buffer, one byte per
// 4-byte word of the vertex record, the attribute word it holds (generic n component c: 0x20 + 4 n + c; 0xff: none).
#pragma once
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t mask;                 // buffers captured (bit b)
    uint32_t stride[4];            // bytes
    uint8_t loc[4][128];           // stride / 4 entries per buffer
} NvnXfbLayout;

// 1 with *out filled when the control section turns transform feedback on; 0 otherwise (or for a layout it cannot
// express: *bad then names why)
static inline int nvn_xfb_layout(const uint8_t *ctl, NvnXfbLayout *out, const char **bad) {
    *bad = NULL;
    if (!ctl || !ctl[0x6ed]) return 0;
    memset(out, 0, sizeof *out);
    for (int b = 0; b < 4; b++) {
        if (!ctl[0x6e8 + b]) continue;
        uint16_t stride;
        memcpy(&stride, ctl + 0x6dc + 2 * b, 2);
        if (!stride || stride % 4 || stride > 512) { *bad = "stride"; return 0; }
        out->mask |= 1u << b, out->stride[b] = stride;
        memset(out->loc[b], 0xff, sizeof out->loc[b]);
        unsigned w = 0;
        for (int i = 0; i < 128; i++) {
            unsigned slot = ctl[0x0dc + 0x80 * b + i], first = ctl[0x2dc + 0x80 * b + i], n = ctl[0x4dc + 0x80 * b + i];
            if (!n) break;
            if (slot < 0x20 || slot >= 0x40 || first + n > 4) { *bad = "output slot"; return 0; }
            for (unsigned c = first; c < first + n; c++) {
                if (w >= stride / 4) { *bad = "entries past the stride"; return 0; }
                out->loc[b][w++] = (uint8_t)(0x20 + 4 * (slot - 0x20) + c);
            }
        }
    }
    return out->mask != 0;
}
