# Tera Recomp plan

## What carries over from TotK Recomp

Generic to any Switch title and reused as is, then extended where Scarlet calls things TotK does not:

- `recomp/`: ARM64 decoder, NEON/FP lowering, jump-table pass, module layout. TotK constants (module list, version
  tables, patch files) become per-title data.
- `runtime/`: Horizon kernel model (threads, 3 cores, preemption), SVCs, IPC, memory with write watch, fs/RomFS,
  audio renderer, input, NVN front end and Vulkan back end, Ryujinx shader translator.
- `tools/`: extraction, hactool build, Ninja build, content check.

## What is TotK-specific and must be redone

- Version identification (`tools/data/expected-v*.json`, `main` hashes) -> Scarlet hashes, taken from the user's dump.
- `recomp/patches-v*.txt`, `runtime/patches.c`: game code patches (frame rate, resolution, zstd) -> none at first.
- `nn::*` replacement list: same SDK family, but Scarlet's SDK version differs; list re-derived from its `sdk` symbols.
- Spin-wait list (`tools/spin_waits.py` output), shader list, RomFS overlays, HUD/aspect layouts.
- Romfs formats: Scarlet uses Trinity (`data.trpfd` / `data.trpfs` packs), not TotK's zstd/sarc.

## Requirements

- **No shader compilation during play.** Every shader and pipeline is built in a preparation step before the game
  starts (as TotK's `prepare_shaders.py` / `--prepare-shaders`):
  - precompiled Maxwell shaders from the RomFS are translated to SPIR-V and compiled ahead;
  - shaders the game builds through `glslc*` at run time are compiled from their GLSL to SPIR-V ahead: sources found
    in the RomFS are compiled directly; variants generated at run time are recorded by a host `glslc` (hash of source
    and options) during development play and shipped as a list the preparation step compiles;
  - pipeline keys seen in play are saved and built at boot.

## Phases

0. Repository, config and content rules. **(done when committed)**
1. Extract ExeFS/RomFS of Scarlet from the user's NSP; record SDK version, module sizes, imports; compare with TotK.
2. Lift all modules to C and compile them (no run yet). Count unhandled instructions and OS sites.
3. Boot: reach `nnMain`, satisfy every `nn::*` / IPC call the game makes until the first NVN submit.
4. First frame on screen; title screen; input.
5. In game: new save, first area; audio; stability runs.
6. Violet on the same build path.
7. Server: replace the online services the game uses (account, NEX/NPLN matchmaking, Link Trade, Link Battle,
   Union Circle) with a self-hosted server; each player keeps a local save.
