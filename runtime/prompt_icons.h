// Keyboard and mouse button prompts (prompt_icons.c): the game's controller button masks known by hash, and our own
// masks of the keys that press them, encoded as BC4 for the texture they replace.
#pragma once
#include <stddef.h>
#include <stdint.h>

int prompt_icons_enabled(void);   // TOTK_KBM_PROMPTS (kbm_prompts, default on)
// The art (enum PromptArt) to show in place of a BC4 texture of w x h whose level-0 blocks are these, or -1.
int prompt_icon_match(const uint8_t *blocks, size_t n, uint32_t w, uint32_t h);
// That art as BC4 blocks for a w x h texture: ((w + 3) / 4) * ((h + 3) / 4) * 8 bytes at out.
void prompt_icon_bc4(int art, uint32_t w, uint32_t h, uint8_t *out);
// TOTK_PROMPT_SURVEY=1 (collecting the game's icons): each unknown mask goes to dump/icons/<hash>_<w>x<h>.bin once.
void prompt_icon_survey(const uint8_t *blocks, size_t n, uint32_t w, uint32_t h);
