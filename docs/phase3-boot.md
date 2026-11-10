# Phase 3: boot log

Scarlet 4.0.0 on the TotK runtime, from the first run on. Each step names what stopped the boot and what fixed it.

| Stop | Cause | Fix |
|---|---|---|
| `fsp-srv` not implemented | `nn::fs::SetAllocator` and seven other fs calls TotK never made | host overrides in `fs_host.c`, `romfs.c` |
| init_array count 2133 != 10717 | boot check with TotK's counts | `RT_SV` / `RT_TOTK` in `host.h` |
| null read in `nn::aoc::CountAddOnContent` | DLC queries TotK never made | overrides in `ov_system.c` |
| NVN `TexturePoolGetSize`, `BufferBuilderGetSize`, ... | 101 NVN procs TotK never called | `nvn_sv.c` (getters, debug controls) |
| `set` service | `nn::settings` language code helpers | overrides in `ov_system.c` |
| null `+0xe0` locked at main+0x14fe800 | `GetEntryType("data:/audio/wwise/bin/Switch/")`: a directory path with its trailing `/` became `...Switch//` and was not found, so the Wwise setup never built its root node | `romfs_entry_type` drops trailing `/` |
| `hid` service | `nn::hid::SetNpadHandheldActivationMode` and other hid calls TotK never made | npad, six-axis and vibration overrides in `ov_input.c` |
| shared fonts render no glyphs | the BFTTF body is XORed with the font key | `font_data` decodes the body for Scarlet |
| `bcat` service | distribution content polled at boot | `ov_bcat.c`: synced, nothing delivered |
| no function at main+0x26ca4 | return point after `bl _setjmp`: lifted code had no setjmp/longjmp | `gen.py` lifts setjmp calls through `rt_setjmp`, `guest_jmp.c` restores registers on `longjmp` |
| `csrng` service | `nn::crypto::GenerateCryptographicallyRandomBytes` | override with the host system RNG (`ov_system.c`) |
| `bt` service | `nn::bluetooth::InitializeBle` (Pokemon GO Plus +) | `ov_bluetooth.c`: Bluetooth without smart devices |
| `audout:u` service | `nn::audio::OpenDefaultAudioOut` (main and subsdk0) | `ov_audout.c`: AudioOut on an SDL3 stream |
| NVN multisample/blend getters | procs TotK never called | `nvn_sv.c` |
| shaders fail: translator not found | `build/xlate/totk_xlate.exe` was never built | `tools/build_xlate.py` (once per checkout) |
| SDK abort in `CreateThread` | `MapAliasStack`: the thread stack lives in main's bss, which MapMemory refused | `memory.c` accepts module data as a stack source |
| `nvnCommandBufferBeginTransformFeedback` | transform feedback passes at boot | **gap**: no-ops that log once; capture is not implemented (needs the program's XFB layout) |

**Milestone (2026-11-09):** first frames, language selection, name and appearance editor, and the opening cutscene with
text, running without a crash. Rendering has many visual errors; the character preview is black (possibly the missing
transform feedback).

## Solved: the null object at main+0x14fe800

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

Crash-time thread dump (`kern_threads_dump`, printed under `guest threads:`): every worker waits on its condition
variable; thread 0x101d (priority 43, code around main+0x339e414) is running a host file read, loading Wwise banks
(`PL.bnk`) at the moment the main thread crashes. The main thread's main+0x14fd2d8 calls no wait function (only
mutexes, `__cxa_guard_*`, bsearch, string functions and nn::audio::AcquireAudioDeviceNotificationForOutput), so the
ordering against the bank loader has to come from earlier in the init chain.

main+0x11267bc (the one `stp` that writes +0x58) does not run before the crash either. Timeline from the log: PM.bnk
opened at 0.488 s, the main thread crashes, PL.bnk opened at 0.506 s. Static search has run out of candidates: the
registry's creator has to be found from a run where the game works (a debugger watchpoint on the singleton's +0x58).

Race test (`TERA_TRACE_SLEEP=11bcec4:3000`, the main thread held 3 s before the failing call with its core given up):
no bank loads during the pause; PM.bnk opens at 3.41 s, right after the main thread resumes, and the crash follows.
The bank loads are started by the failing call itself, so this is not a race with a loader thread: the registry at the
singleton's +0x58 is created by an initialization step that never runs on the host. Finding it needs a run where the
game works: a write watchpoint on the singleton's +0x58 in an emulator's debugger.

Found: main+0x1508a4c checks the Wwise bank directory `data:/audio/wwise/bin/Switch/` through main+0x15096ec, which
asks GetEntryType. runtime/romfs.c appended '/' to a path that already ended in one, so the directory was not found,
main+0x1505338 stopped before building the root node and the find-or-create in main+0x14ff4dc returned null. With the
trailing '/' handled the root is built and the boot goes on (the Wwise children are created, then the hid service).
The comparison with Ryujinx (tools/gdb/inspect.py) pointed at the missing root node.
