// Keyboard and mouse button prompts for Scarlet and Violet (docs/controls.md, "Button prompts follow the device").
//
// The game draws each controller button of its prompts (A, B, X, Y, ZL, ZR, Plus, Minus, the right stick) from a
// texture of its own: a small BC4 mask, a white shape with the label cut out, which the UI tints. vk_texture.c hands
// every small BC4 texture it uploads to prompt_icon_match, which knows those masks by the FNV-1a hash of their blocks
// (the deswizzled level 0, as the texture holds it) and answers with the key or mouse button that the default
// keyboard bindings (input_map.c SV_KEYS) press it with. While keyboard and mouse are the device used last
// (input_kbm_last), vk_texture.c writes our own mask of that key into the texture (prompt_icon_bc4), and the game's
// own blocks back once the gamepad is used. Our masks (prompt_icons_art.h, tools/make_prompt_icons.py) are drawn for
// the port; only hashes of the game's are kept here.
#include <stdint.h>
#include <string.h>

#include "cfg.h"
#include "prompt_icons.h"
#include "prompt_icons_art.h"

typedef struct { uint64_t hash; uint16_t w, h; uint8_t art; } Known;

// The masks seen in the 4.0.0 UI (the map screen), by FNV-1a 64 of their level-0 blocks. Their buttons, and the keys
// of SV_KEYS that press them: A E, B Q, X Tab, Y Space, ZL right mouse button, ZR left mouse button, Plus R, Minus M,
// the right stick the mouse, its click the middle button.
static const Known known[] = {
    {0x23aa6e7169a0df59ull, 32, 32, PROMPT_KEY_E},        // A
    {0xc65d9ebf80426894ull, 32, 32, PROMPT_KEY_Q},        // B
    {0xe5948a375be1d2a8ull, 32, 32, PROMPT_KEY_TAB},      // X
    {0xab7c0d17e82d332eull, 32, 32, PROMPT_KEY_SPACE},    // Y
    {0x180b485f97508f56ull, 32, 32, PROMPT_MOUSE_R},      // ZL
    {0x69299763fb39a821ull, 32, 32, PROMPT_MOUSE_L},      // ZR
    {0xe95935a7c0c962acull, 20, 20, PROMPT_KEY_R},        // Plus
    {0x8e8d1c4c02a0ec80ull, 20, 20, PROMPT_KEY_M},        // Minus
    {0x3f13cac4dc099f25ull, 32, 32, PROMPT_MOUSE_MOVE},   // right stick (camera)
    {0x25794d37de777ee6ull, 40, 40, PROMPT_MOUSE_M},      // right stick pressed
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

int prompt_icon_match(const uint8_t *blocks, size_t n, uint32_t w, uint32_t h) {
    if (n != (size_t)((w + 3) / 4) * ((h + 3) / 4) * 8) return -1;
    uint64_t x = fnv(blocks, n);
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (known[i].hash == x && known[i].w == w && known[i].h == h) return known[i].art;
    return -1;
}

// The 64x64 art reduced to w x h by area: each output texel averages the art texels its square covers.
static uint8_t sample(int art, uint32_t w, uint32_t h, uint32_t x, uint32_t y) {
    const uint8_t *a = prompt_art[art];
    uint32_t x0 = x * PROMPT_ART_SIZE / w, x1 = (x + 1) * PROMPT_ART_SIZE / w;
    uint32_t y0 = y * PROMPT_ART_SIZE / h, y1 = (y + 1) * PROMPT_ART_SIZE / h;
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

void prompt_icon_bc4(int art, uint32_t w, uint32_t h, uint8_t *out) {
    uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    for (uint32_t by = 0; by < bh; by++)
        for (uint32_t bx = 0; bx < bw; bx++) {
            uint8_t v[16];
            for (int i = 0; i < 16; i++) {
                uint32_t x = bx * 4 + (uint32_t)(i & 3), y = by * 4 + (uint32_t)(i >> 2);
                v[i] = x < w && y < h ? sample(art, w, h, x, y) : 0;
            }
            bc4_block(v, out + (by * bw + bx) * 8);
        }
}
