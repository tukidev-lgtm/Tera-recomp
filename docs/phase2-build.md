# Phase 2: lift and link Scarlet 4.0.0

Done on the development machine (16 threads, Windows, MSYS2 UCRT64 Clang 23.1.5).

## Lift (`recomp/gen.py`, no code patches)

| Module | Functions | Chunks | C | Jump tables | All-label fallbacks | `br` via dispatch | Traps | Unsupported |
|---|---|---|---|---|---|---|---|---|
| main | 180,337 | 337 | 939 MB | 4,390 | 31 | 18,238 | 54,847 | 0 |
| sdk | 39,287 | 40 | 106 MB | 661 | 16 | 10,327 | 24,661 | 0 |
| subsdk0 | 13,662 | 24 | 59 MB | 335 | 1 | 3,957 | 5,589 | 0 |

Every instruction in the three modules decodes and lowers to C. Traps are `BRK`/`UDF` sites the game itself contains.
With one worker the lift takes 555 s; with the two workers `gen.py` chose from 4.6 GiB of free memory, the workers
died partway through. Run it with `--jobs 1` (or `TOTK_LIFT_JOBS=1`) until the per-worker memory estimate is checked
against `main`'s largest chunks.

## Compile and link (`tools/build_game.py --version v400 --no-romfs --jobs 6`)

- 525 objects (main 338, sdk 41, subsdk0 25, runtime 121), 0 errors, 520.8 s, peak 340 MiB per clang process.
- `build/v400/totk.exe`: 240.5 MiB file, 206.3 MiB of `.text`.
- The default job count is the CPU count bounded by 80% of total RAM; on a machine with little free memory pass
  `--jobs` explicitly.

The executable links but has not run: it has no RomFS index yet (`--no-romfs`) and the runtime still carries TotK's
boot assumptions. That is phase 3.
