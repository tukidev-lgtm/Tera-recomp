// Keyboard and mouse button prompts (prompt_icons.c): the game's controller button masks known by hash, and our own
// masks of the keys that press them, encoded as BC4 for the texture they replace.
#pragma once
#include <stddef.h>
#include <stdint.h>

int prompt_icons_enabled(void);   // TOTK_KBM_PROMPTS (kbm_prompts, default on)
// The known icon (an index) whose level-0 blocks are these, for a w x h texture of Vulkan format fmt (BC4 or BC5), or -1.
int prompt_icon_match(const uint8_t *blocks, size_t n, uint32_t w, uint32_t h, int fmt);
// The bytes of that icon's level 0, and our mask of its key encoded in its format into out (that many bytes).
size_t prompt_icon_bytes(int icon);
void prompt_icon_encode(int icon, uint8_t *out);
// TOTK_PROMPT_SURVEY (collecting the game's icons): an unknown texture goes to dump/icons/<hash>_<w>x<h>_vk<fmt>.bin
// once; prompt_icon_surveying is the largest side it takes, 0 when off.
void prompt_icon_survey_fmt(const uint8_t *data, size_t n, uint32_t w, uint32_t h, int vk_format);
int prompt_icon_surveying(void);
