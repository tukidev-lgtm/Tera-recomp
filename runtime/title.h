// The game this build runs: its program id, in the forms the runtime needs. One place, so the fs (save data), the
// mods folder layout and svcGetInfo agree. Scarlet by default; -DTERA_VIOLET builds for Violet.
#pragma once

#ifdef TERA_VIOLET
#define TITLE_PROGRAM_ID 0x01008f6008c5e000ull
#define TITLE_ID_STR "01008F6008C5E000"
#else
#define TITLE_PROGRAM_ID 0x0100a3d008c5c000ull
#define TITLE_ID_STR "0100A3D008C5C000"
#endif
