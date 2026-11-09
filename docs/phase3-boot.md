# Phase 3: boot log

Scarlet 4.0.0 on the TotK runtime, from the first run on. Each step names what stopped the boot and what fixed it.

| Stop | Cause | Fix |
|---|---|---|
| `fsp-srv` not implemented | `nn::fs::SetAllocator` and seven other fs calls TotK never made | host overrides in `fs_host.c`, `romfs.c` |
| init_array count 2133 != 10717 | boot check with TotK's counts | `RT_SV` / `RT_TOTK` in `host.h` |
| null read in `nn::aoc::CountAddOnContent` | DLC queries TotK never made | overrides in `ov_system.c` |
| NVN `TexturePoolGetSize`, `BufferBuilderGetSize`, ... | 101 NVN procs TotK never called | `nvn_sv.c` (getters, debug controls) |
| `set` service | `nn::settings` language code helpers | overrides in `ov_system.c` |
| null `+0xe0` locked at main+0x14fe800 | open, see below | |

## Open: the null object at main+0x14fe800

Boot path: `nnMain` -> main+0x165e87c -> main+0x92bdc0 -> main+0x11acbb0 -> main+0x11bcc84 (init chain).

- main+0x11bcc84 calls slot +0x58 of the Trinity file/archive manager (vtable main+0x443ffd0, the object that mounts
  the RomFS and reads `arc/data.trpfd` in main+0x11e1a60): main+0x14fd2d8.
- main+0x14fd2d8 hands an inner object (vtable main+0x4500fc0, multiple inheritance) to main+0x14ff4dc, which reads
  the inner object's +0x58, finds it empty and returns null; main+0x14fd2d8 then locks result+0xe0.
- The inner object is freshly constructed: +0x38..+0x78 are all zero.
- Ruled out: main+0x11e1a60's feature flag (main+0x4759428 reads 1; that block runs and fills another object).

Later runs narrowed it down:

- The outer object is the Trinity file/archive manager (vtable main+0x443ffd0); main+0x14fd2d8 is its slot +0x58.
- The inner object is a global singleton (slot main+0x46d8288) of the Wwise sound engine's setup: the surrounding code
  names `Wwise_Motion`, `audio/fb/...` config paths and `bankName%d`.
- main+0x14ff4dc looks an entry up by FNV-1a 64 hash in a registry held at the singleton's +0x58. The registry is
  never created: none of the 33 stores to +0x58 in the 139 functions that load the singleton runs before the crash.

Working theory: the registry is filled when the audio configs are loaded from the pack, on a worker thread that on
the console finishes before this step and on the host has not run or is blocked.
