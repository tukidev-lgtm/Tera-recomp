# Tera Recomp

Static recompilation of *Pokémon Scarlet* (and later *Violet*) for the Nintendo Switch into a native x86-64 Windows
program with a Vulkan renderer, followed by a self-hosted multiplayer server (trades, battles, Union Circle) for many
players at once, each on their own save and at their own pace.

> **No game files are included.** You need your own legally obtained copy, dumped from your own console, and the keys
> of that console. The game is built from your files on your machine. Do not share the result.

## Status

Phase 0: project set up from [TotK Recomp](https://github.com/valentderah/totk-pc-recomp) (MIT), whose generic Switch
parts (ARM64 lifter, Horizon kernel model, NVN on Vulkan, shader translation) are the starting point. See
[docs/plan.md](docs/plan.md) for the roadmap and what is TotK-specific and has to be redone.

## Targets

| Game | Title id (base) | Update title id |
|---|---|---|
| Pokémon Scarlet | `0100A3D008C5C000` | `0100A3D008C5C800` |
| Pokémon Violet  | `01008F6008C5E000` | `01008F6008C5E800` |

## Credits and license

Built on TotK Recomp and the projects it credits ([docs/totk-README.md](docs/totk-README.md),
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). Own code under the MIT License. The license grants nothing for
Nintendo's, The Pokémon Company's or Game Freak's game. Independent, non-commercial fan project, not affiliated with
or endorsed by them. *Pokémon* is a trademark of Nintendo, Creatures Inc. and GAME FREAK inc.
