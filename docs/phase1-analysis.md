# Phase 1: ExeFS survey of Scarlet 4.0.0

Produced by `tools/sv_analyze.py` from the user's own dump (`build/extracted/v400/exefs`). Only counts and version
identifiers are recorded here; the per-symbol output stays in `build/analysis/` and is never committed.

## Version

| | |
|---|---|
| Title | Pokémon Scarlet `0100A3D008C5C000`, update 4.0.0 (`0100A3D008C5C800`, v786432) |
| `main` SHA-256 | `f5f4771ff1e37ac55dcfe5698aa956a4762184b864727ed61e74b1867d1dbfc2` |
| `main` build id | `421c5411b487eb4d049dd065fec9547773e8e598` |
| `sdk` build id | `bb560bdfa85b4a97786ae2c3b7995df6d58f5567` |
| `subsdk0` build id | `310fad4782253cab20b89d4c497a8d16e8bd72c4` (NintendoSDK movie) |
| NintendoSDK | 13.3.13 (TotK 1.4.3 uses 20.5.22) |
| Online stack in `main` | NintendoSDK NPLN 1.8.3, Google APIs for NPLN (gRPC), OpenSSL for NPLN |
| Other middleware | Havok AI 2021.1, nlib LZ4 |

## Modules

| Module | .text | .rodata | .data | .bss | Defined syms | Imported syms | Relocations |
|---|---|---|---|---|---|---|---|
| main | 52.2 MiB | 15.3 MiB | 3.4 MiB | 0.9 MiB | 68 | 1013 | 283,698 |
| sdk | 5.8 MiB | 6.7 MiB | 0.4 MiB | 0.7 MiB | 27,157 | 13 | 34,145 |
| subsdk0 | 3.2 MiB | 2.1 MiB | 0.2 MiB | 1.1 MiB | 10,882 | 659 | 19,183 |

Same module set as TotK (`main`, `sdk`, `subsdk0`, `rtld`), so `recomp/layout.py` and the loader apply unchanged.
`main` is stripped (68 exported symbols): game functions have no names, only the SDK boundary does.

## OS surface

The game modules import 1,460 symbols from `sdk`, 632 of them in `nn::`.

- **89** are already replaced by host functions in the runtime inherited from TotK.
- **543** are not. Most of them are plain library code that runs lifted as is (`nn::util` 44, `nn::mem` 13,
  `nn::crypto` 12, `nn::fontll` 24, `nn::lmem` 6, `nn::sf` 12, `nn::diag` 8) or reaches the kernel through SVCs the
  runtime already models (`nn::os` 102). The rest talk to Horizon services over IPC, and in the TotK design only
  `sm:` and `audren:u` are real IPC services: each of these needs a host override or a new service.
- Only 4 of the 209 runtime overrides name a function Scarlet's SDK lacks, so SDK 13 kept almost every name TotK
  relies on. Struct layouts behind those names still have to be checked: 13.3.13 is older than every TotK build.

Not replaced, by namespace, for the services TotK never needed:

| Namespace | Calls | Needed for |
|---|---|---|
| `nn::hid` (rest) | 45 | input beyond what TotK reads |
| `nn::audio` (rest) | 42 | audio |
| `nn::bluetooth` | 34 | none offline; stub |
| `nn::socket` | 29 | online, phase 7 |
| `nn::ldn` | 27 | local wireless play; phase 7 (can carry Union Circle over the server) |
| `nn::bcat` | 22 | events/distributions; stub, later served |
| `nn::account` (rest) | 19 | user selection, network account |
| `nn::err` | 19 | error dialogs |
| `nn::ssl`, `nn::nifm`, `nn::nsd`, `nn::web`, `nn::ec` | 27 | online; stub until phase 7 |

`main.npdm` lists 49 services the game may open (TotK's runtime serves two over IPC and the rest at the `nn::` level).

## Findings that change the plan

1. **Runtime shader compiler.** `main` imports eight `glslc*` functions (`glslcInitialize`, `glslcCompile`, ...) that
   no module defines: the game can compile GLSL at run time through a library the system loads. TotK only ships
   precompiled Maxwell shaders. A host `glslc` that compiles GLSL straight to SPIR-V (shaderc is already in the
   toolchain) could be simpler than translating Maxwell for those shaders. To check: whether those calls happen in
   normal play or only in a debug path.
2. **Online is NPLN over gRPC/TLS**, not the older NEX protocol. The phase 7 server speaks NPLN's gRPC services;
   `nn::ldn` (local play) is the other path into trades and Union Circle and may be easier to redirect to a server.
3. **Code size.** 52 MiB of `.text` in `main` and 283,698 relocations: the lifter's per-worker memory bounds matter;
   lifting runs on the Windows machine, not in a small VM.
4. **Romfs** is Trinity (`arc/data.trpfd`, `arc/data.trpfs`): `runtime/romfs.c` serves files by path and needs no
   format knowledge, but TotK's zstd/restbl/sarc overlays (`restbl.c`, `ov_zstd.c`, romfs patches) do not apply.

## Next: phase 2

Lift `main`, `sdk` and `subsdk0` with `recomp/gen.py` (no TotK patches), count unhandled instructions and OS sites,
then compile the generated C with the runtime. Nothing runs yet; the goal is a clean link.
