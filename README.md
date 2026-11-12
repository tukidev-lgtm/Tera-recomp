# Tera Recomp

Static recompilation of *Pokémon Scarlet* (and later *Violet*) for the Nintendo Switch into a native x86-64 Windows
program with a Vulkan renderer, followed by a self-hosted multiplayer server (trades, battles, Union Circle) for many
players at once, each on their own save and at their own pace.

> **No game files are included.** You need your own legally obtained copy, dumped from your own console, and the keys
> of that console. The game is built from your files on your machine. Do not share the result.

[![Follow @Tukidev on X for updates](https://img.shields.io/badge/updates-%40Tukidev-000000?style=for-the-badge&logo=x)](https://x.com/Tukidev)
[![Download Tera-Setup-Windows-x64.zip](https://img.shields.io/github/v/release/tukidev-lgtm/Tera-recomp?style=for-the-badge&label=download)](https://github.com/tukidev-lgtm/Tera-recomp/releases/latest/download/Tera-Setup-Windows-x64.zip)
[![Verified developer on RecompStore](https://recompstore.com/assets/press/verified-dark.svg)](https://recompstore.com/dev/tukidev-lgtm)
<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

By [Tukidev](https://x.com/Tukidev): follow for progress updates · [Download](https://github.com/tukidev-lgtm/Tera-recomp/releases/latest/download/Tera-Setup-Windows-x64.zip) · [Report a bug](https://github.com/tukidev-lgtm/Tera-recomp/issues/new)

## Download and install (Windows)

**[Download Tera-Setup-Windows-x64.zip](https://github.com/tukidev-lgtm/Tera-recomp/releases/latest/download/Tera-Setup-Windows-x64.zip)**
(about 145 MB). All releases: [Releases](https://github.com/tukidev-lgtm/Tera-recomp/releases).

The download holds the setup and its tools only. The setup builds the game on your PC from your own files, once.

With [RecompLauncher](https://recomplauncher.com): add Tera Recomp, point it to one folder holding your base game,
the 4.0.0 update, `prod.keys` and the firmware folder, and it builds and starts the game for you
(`recomplauncher.json` describes the setup, the settings and the saves). Or set it up yourself:

### What you need

- Windows 10 or 11, 64-bit; a graphics card with Vulkan 1.3 (NVIDIA GTX 900 / AMD RX 400 / Intel Arc or newer, with
  current drivers).
- About 30 GB free on the disk you install to while the setup runs (12 GB after it).
- From your own Switch:
  - the **Pokémon Scarlet base game** (NSP, XCI, NSZ or XCZ);
  - the **4.0.0 update** (NSP or NSZ);
  - your console's **`prod.keys`**;
  - the **firmware folder** (`Contents/registered`, full of `.nca` files): the game draws all its text with the
    console's system fonts, which the setup takes from there. An emulator's firmware install works.

### Steps

1. Download `Tera-Setup-Windows-x64.zip` and unzip it anywhere (right click > *Extract All*). Keep the folder; the setup uses it
   again when you update.
2. Open `Tera-Setup.exe`. If Windows SmartScreen warns about an unknown publisher, click *More info* > *Run anyway*.
3. In the window, choose:
   - **Base game**: the Scarlet base game file;
   - **Update 4.0.0**: the update file;
   - **prod.keys**: your keys file (read in place, never copied or shown);
   - **Firmware folder**: the `Contents/registered` folder;
   - **Game language**;
   - **Install folder**: a new, empty folder, for example `C:\Games\Tera Recomp`.
4. Click **Start** and wait. The window shows each step: checking the files, extracting the system fonts, extracting
   and merging the game with the update, translating the game code to C and compiling it. About 20 minutes on a
   recent PC. If it stops, it resumes from the last finished step when you click Start again.
5. When it is done, start the game with the **Pokemon Scarlet** shortcut on your desktop (or `totk.exe` in the install
   folder).

The first time you meet a new area or effect the game translates its shaders, which can stutter briefly; from the
second run on they are compiled at start.

## Status

Early alpha. The game boots, runs the intro and character creation with the opening cutscene and its audio, and can be
walked around the starting area at 30 fps with correct lighting. Expect crashes and graphics bugs further in. Multiplayer (the
self-hosted server) is not there yet: see [docs/plan.md](docs/plan.md) and the boot log in
[docs/phase3-boot.md](docs/phase3-boot.md).

## Mods

Mods go in the `mods` folder next to `totk.exe`, one folder per mod. The settings menu's Mods tab opens that folder
and switches mods on and off. The mod system comes from TotK Recomp and has not been tested with Scarlet mods yet:
[docs/mods.md](docs/mods.md).

Expected to work:

- **File mods** (`romfs`): textures, models, outfits, Pokémon and battle data, text and music, including mods that ship
  a modified `data.trpfd` with loose files.
- **Code patches** (`.ips`, `.pchtxt`) made for game version 4.0.0. The game rebuilds itself with them at the next
  start.
- **Atmosphère cheats** (`cheats`).

Not supported:

- **Mods with their own code**: a `subsdk` plugin (exlaunch and similar) or a replaced `main`. Such a mod stays off as
  a whole. Pokémon Ultimate Scarlet is one of them. These plugins hook the game by rewriting its instructions in
  memory, and the port runs the game as recompiled native code, so those rewrites do nothing.
- **Code patches for another game version** (for example 3.0.1). They are skipped.

## Report a problem

[Open an issue](https://github.com/tukidev-lgtm/Tera-recomp/issues/new) with:

- what you did and what happened (a screenshot helps a lot);
- your graphics card and Windows version;
- the log: `logs/totk.log` in the install folder for a game problem, or the setup log the window names (*Show log*)
  for a setup problem.

Never attach game files or keys.

News and progress: [@Tukidev on X](https://x.com/Tukidev).

## Targets

| Game | Title id (base) | Update title id |
|---|---|---|
| Pokémon Scarlet | `0100A3D008C5C000` | `0100A3D008C5C800` |
| Pokémon Violet  | `01008F6008C5E000` | `01008F6008C5E800` |

## Building from source

Developers: [docs/building.md](docs/building.md) (toolchain), `tools/dev_run.ps1` (build and run from the
repository), `tools/make_release.py` (the setup package).

## Credits and license

Built on TotK Recomp and the projects it credits ([docs/totk-README.md](docs/totk-README.md),
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). Own code under the MIT License. The license grants nothing for
Nintendo's, The Pokémon Company's or Game Freak's game. Independent, non-commercial fan project, not affiliated with
or endorsed by them. *Pokémon* is a trademark of Nintendo, Creatures Inc. and GAME FREAK inc.
