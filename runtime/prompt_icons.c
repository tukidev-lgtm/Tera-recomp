// Keyboard and mouse button prompts for Scarlet and Violet (docs/controls.md, "Button prompts follow the device").
//
// The game draws each controller button of its prompts (A, B, X, Y, ZL, ZR, Plus, Minus, the right stick) from a
// texture of its own: a small BC4 mask (a white shape with the label cut out) or BC5 one (a disc in one channel, the
// label cut out of the other), which the UI tints. vk_texture.c hands every small BC4 and BC5 texture it uploads to
// prompt_icon_match, which knows those masks by the FNV-1a hash of their blocks
// (the deswizzled level 0, as the texture holds it) and answers with the key or mouse button that the default
// keyboard bindings (input_map.c SV_KEYS) press it with. While keyboard and mouse are the device used last
// (input_kbm_last), vk_texture.c writes our own mask of that key into the texture (prompt_icon_encode), and the game's
// own blocks back once the gamepad is used. Our masks (prompt_icons_art.h, tools/make_prompt_icons.py) are drawn for
// the port; only hashes of the game's are kept here.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cfg.h"
#include "host.h"
#include "platform/platform.h"
#include "prompt_icons.h"
#include "prompt_icons_art.h"

typedef struct { uint64_t hash; uint16_t w, h, fmt; uint8_t art, x, y, bw, bh; } Known;
enum { BC4 = 139, BC5 = 141 };   // VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK

// The masks seen in the 4.0.0 UI, by FNV-1a 64 of their level-0 blocks, with the box their button's disc fills. Their
// buttons, and the keys of SV_KEYS that press them: A E, B Q, X Tab, Y Space, L Z, R C, ZL right mouse button, ZR left
// mouse button, Plus R, Minus M, the right stick the mouse, its click the middle button. One-channel (BC4) icons are a white
// shape with the label cut out; two-channel (BC5) ones hold the disc in G and the label cut out of R.
static const Known known[] = {
    {0x23aa6e7169a0df59ull, 32, 32, BC4, PROMPT_KEY_E, 0, 0, 32, 32},        // A
    {0xc65d9ebf80426894ull, 32, 32, BC4, PROMPT_KEY_Q, 0, 0, 32, 32},        // B
    {0xe5948a375be1d2a8ull, 32, 32, BC4, PROMPT_KEY_TAB, 0, 0, 32, 32},      // X
    {0xab7c0d17e82d332eull, 32, 32, BC4, PROMPT_KEY_SPACE, 0, 0, 32, 32},    // Y
    {0x180b485f97508f56ull, 32, 32, BC4, PROMPT_MOUSE_R, 0, 0, 32, 32},      // ZL
    {0x69299763fb39a821ull, 32, 32, BC4, PROMPT_MOUSE_L, 0, 0, 32, 32},      // ZR
    {0xe95935a7c0c962acull, 20, 20, BC4, PROMPT_KEY_R, 0, 0, 20, 20},        // Plus
    {0x8e8d1c4c02a0ec80ull, 20, 20, BC4, PROMPT_KEY_M, 0, 0, 20, 20},        // Minus
    {0x8edafa608a25d09cull, 32, 32, BC4, PROMPT_KEY_R, 0, 0, 32, 32},        // Plus in a disc
    {0x4b2d6a04a9553b70ull, 32, 32, BC4, PROMPT_KEY_M, 0, 0, 32, 32},        // Minus in a disc
    {0x3f13cac4dc099f25ull, 32, 32, BC4, PROMPT_MOUSE_MOVE, 0, 0, 32, 32},   // right stick (camera)
    {0x25794d37de777ee6ull, 40, 40, BC4, PROMPT_MOUSE_M, 0, 0, 40, 40},      // right stick pressed
    {0x4eb2b3ca37ba9d08ull, 40, 40, BC4, PROMPT_KEY_Z, 1, 4, 38, 32},        // L (the box tabs)
    {0x26b1270192722106ull, 40, 40, BC4, PROMPT_KEY_C, 1, 4, 38, 32},        // R (the box tabs, Save in the menu)
    {0x5da9c4289de85445ull, 52, 70, BC5, PROMPT_KEY_TAB, 1, 10, 50, 50},     // X, large
    {0xca160275ec218944ull, 52, 70, BC5, PROMPT_KEY_SPACE, 1, 10, 50, 50},   // Y, large
};

int prompt_icons_enabled(void) {
    static int on = -1;
    if (on < 0) on = cfg_bool("TOTK_KBM_PROMPTS");
    return on;
}

static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static size_t bytes_of(int fmt, uint32_t w, uint32_t h) { return (size_t)((w + 3) / 4) * ((h + 3) / 4) * (fmt == BC5 ? 16 : 8); }

int prompt_icon_match(const uint8_t *blocks, size_t n, uint32_t w, uint32_t h, int fmt) {
    if (n != bytes_of(fmt, w, h)) return -1;
    uint64_t x = fnv(blocks, n);
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (known[i].hash == x && known[i].w == w && known[i].h == h && known[i].fmt == fmt) return (int)i;
    return -1;
}

// Mask m (0 shape, 1 cut) of the art at texel (x, y) of the icon's box, reduced by area: each texel averages the art
// texels its square covers; 0 outside the box.
static uint8_t sample(const Known *k, int m, uint32_t x, uint32_t y) {
    if (x < k->x || y < k->y || x >= (uint32_t)k->x + k->bw || y >= (uint32_t)k->y + k->bh) return 0;
    x -= k->x, y -= k->y;
    const uint8_t *a = prompt_art[k->art][m];
    uint32_t x0 = x * PROMPT_ART_SIZE / k->bw, x1 = (x + 1) * PROMPT_ART_SIZE / k->bw;
    uint32_t y0 = y * PROMPT_ART_SIZE / k->bh, y1 = (y + 1) * PROMPT_ART_SIZE / k->bh;
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    uint32_t sum = 0, n = 0;
    for (uint32_t j = y0; j < y1 && j < PROMPT_ART_SIZE; j++)
        for (uint32_t i = x0; i < x1 && i < PROMPT_ART_SIZE; i++) sum += a[j * PROMPT_ART_SIZE + i], n++;
    return (uint8_t)(n ? (sum + n / 2) / n : 0);
}

// One BC4 block of 16 values: the 8-value mode (r0 > r1) between the block's extremes, each texel at its nearest step.
static void bc4_block(const uint8_t v[16], uint8_t out[8]) {
    uint8_t lo = 255, hi = 0;
    for (int i = 0; i < 16; i++) lo = v[i] < lo ? v[i] : lo, hi = v[i] > hi ? v[i] : hi;
    memset(out, 0, 8);
    out[0] = hi, out[1] = lo;
    if (hi == lo) return;   // every index 0: the value hi
    uint64_t bits = 0;
    for (int i = 0; i < 16; i++) {
        // steps 0..7 from hi to lo: 0 is hi, 1 is lo, 2..7 the six between (6 hi + 1 lo)/7 .. (1 hi + 6 lo)/7
        int t = ((hi - v[i]) * 7 + (hi - lo) / 2) / (hi - lo);   // 0 = hi .. 7 = lo
        int idx = t == 0 ? 0 : t == 7 ? 1 : t + 1;
        bits |= (uint64_t)idx << (3 * i);
    }
    for (int i = 0; i < 6; i++) out[2 + i] = (uint8_t)(bits >> (8 * i));
}

size_t prompt_icon_bytes(int icon) { return bytes_of(known[icon].fmt, known[icon].w, known[icon].h); }

void prompt_icon_encode(int icon, uint8_t *out) {
    const Known *k = &known[icon];
    uint32_t bw = (k->w + 3u) / 4, bh = (k->h + 3u) / 4, bs = k->fmt == BC5 ? 16 : 8;
    for (uint32_t by = 0; by < bh; by++)
        for (uint32_t bx = 0; bx < bw; bx++) {
            uint8_t one[16], r[16], g[16];
            for (int i = 0; i < 16; i++) {
                uint32_t x = bx * 4 + (uint32_t)(i & 3), y = by * 4 + (uint32_t)(i >> 2);
                int in = x < k->w && y < k->h;
                uint8_t shape = in ? sample(k, 0, x, y) : 0, cut = in ? sample(k, 1, x, y) : 0;
                one[i] = (uint8_t)(shape * (255 - cut) / 255);   // the shape less the cut
                r[i] = (uint8_t)(255 - cut), g[i] = shape;       // two-channel: the label cut out of R, the disc in G
            }
            uint8_t *o = out + (by * bw + bx) * bs;
            if (k->fmt == BC5) bc4_block(r, o), bc4_block(g, o + 8);
            else bc4_block(one, o);
        }
}

// The textures the survey wrote, by hash: each goes to disk once per run.
// The largest side the survey takes (TOTK_PROMPT_SURVEY: 0 off, 1 128 texels, else that many), 0 when off.
int prompt_icon_surveying(void) {
    static int side = -1;
    if (side < 0) {
        side = cfg_int("TOTK_PROMPT_SURVEY");
        if (side == 1) side = 128;
        if (side < 0) side = 0;
        if (side) plat_file_mkdir_p("dump/icons"), rt_log("prompts: survey on: unknown textures up to %d texels a side go to dump/icons", side);
    }
    return side;
}

void prompt_icon_survey_fmt(const uint8_t *data, size_t n, uint32_t w, uint32_t h, int vk_format) {
    static PlatMutex lock = PLAT_MUTEX_INIT;
    static int nseen;
    static uint64_t seen[8192];
    if (!prompt_icon_surveying()) return;
    uint64_t x = fnv(data, n) ^ (uint64_t)vk_format;
    plat_mutex_lock(&lock);
    int fresh = nseen < 8192;
    for (int i = 0; i < nseen && fresh; i++) fresh = seen[i] != x;
    if (fresh) seen[nseen++] = x;
    plat_mutex_unlock(&lock);
    if (!fresh) return;
    char path[112];
    snprintf(path, sizeof path, "dump/icons/%016llx_%ux%u_vk%d.bin", (unsigned long long)x, w, h, vk_format);
    plat_file_write(path, data, n);
}

