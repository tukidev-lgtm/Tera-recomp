// The game this build runs: its program id, in the forms the runtime needs. One place, so the fs (save data), the
// mods folder layout, add-on content and svcGetInfo agree. tools/build_game.py --title writes both into modules.c,
// so one prebuilt runtime library serves Scarlet and Violet.
#pragma once
#include <stdint.h>

extern const uint64_t rt_title_program_id;   // Scarlet 0x0100A3D008C5C000, Violet 0x01008F6008C5E000
extern const char rt_title_id_str[];          // the same as 16 upper-case hex digits
#define TITLE_PROGRAM_ID rt_title_program_id
#define TITLE_ID_STR rt_title_id_str
