"""--game <folder>: find the setup's inputs in one folder the player points to (RecompLauncher passes it as {game}).

The folder holds the player's own files, at its top or in subfolders up to MAX_DEPTH deep:
    the base game and the update     .nsp, .xci, .nsz or .xcz; told apart by the title ID of their Meta NCA (an update's
                                     ends in 800), read with the keys, else by the name ("UPD", "[v0]")
    the DLC (optional)               an .nsp whose Meta NCA's title ID has other low bits (add-on content)
    prod.keys                        the console's keys (any *.keys file named prod.keys first)
    the firmware                     the folder with the most .nca files (Contents/registered of a firmware dump or an
                                     emulator's install)
Nothing is copied or written; the paths fill the options the player did not give on the command line.
"""
import os
from pathlib import Path

GAME_EXT = ('.nsp', '.xci', '.nsz', '.xcz')
MAX_DEPTH = 4
FIRMWARE_MIN_NCAS = 20


class GameFolderError(ValueError):
    pass


def walk(folder):
    """(directory, file names) of folder and its subfolders up to MAX_DEPTH, in a stable order."""
    base = len(folder.parts)
    for root, dirs, files in os.walk(folder):
        here = Path(root)
        dirs.sort()
        if len(here.parts) - base >= MAX_DEPTH:
            dirs[:] = []
        yield here, sorted(files)


def meta_title(path, keys):
    """The title ID of the package's Meta NCA, or None when it cannot be read with these keys."""
    from extract_exefs import container_entries, nca_inventory   # tools/vendor, on sys.path through installer.steps
    try:
        with path.open('rb') as f:
            _, entries = container_entries(f, path.stat().st_size)
            meta = [r['title_id'] for r, _ in nca_inventory(f, entries, keys) if r['content_type'] == 1]
        return meta[0] if len(meta) == 1 else None
    except (OSError, ValueError, KeyError):
        return None


def by_name(path):
    """'update', 'base' or None from the file name alone (the usual dump names)."""
    n = path.name.lower()
    if 'upd' in n or '800]' in n or 'update' in n:
        return 'update'
    if 'base' in n or '[v0]' in n or '000]' in n:
        return 'base'
    return None


def find(folder):
    """{'base', 'update', 'keys', 'firmware', 'dlc'} paths found in folder ('dlc' may be None); raises GameFolderError
    naming what is missing."""
    folder = Path(folder)
    if not folder.is_dir():
        raise GameFolderError(f'{folder} is not a folder')
    games, keys, firmware, most = [], [], None, FIRMWARE_MIN_NCAS - 1
    for here, files in walk(folder):
        games += [here / n for n in files if n.lower().endswith(GAME_EXT)]
        keys += [here / n for n in files if n.lower().endswith('.keys')]
        ncas = sum(1 for n in files if n.lower().endswith('.nca'))
        if ncas > most:
            firmware, most = here, ncas
    keyfile = next((k for k in keys if k.name.lower() == 'prod.keys'), keys[0] if keys else None)
    found = {'keys': keyfile, 'firmware': firmware, 'base': None, 'update': None, 'dlc': None}
    loaded = None
    if keyfile:
        from extract_exefs import load_keys
        try:
            loaded = load_keys(keyfile)
        except (OSError, ValueError, UnicodeDecodeError):
            loaded = None
    # Each file by its Meta NCA's title ID: the application's own (...000) is the base game, bit 11 set (...800) its
    # update, any other low bits add-on content. With several games in the folder the base and update are taken from
    # the first game that has both, and the DLC from the same game.
    kinds = []
    for g in games:
        title = meta_title(g, loaded) if loaded else None
        if title:
            low = int(title, 16) & 0xFFF
            app = f'{int(title, 16) & ~0xFFF:016X}' if low in (0, 0x800) else f'{(int(title, 16) & ~0xFFF) - 0x1000:016X}'
            kinds.append((g, 'base' if low == 0 else 'update' if low == 0x800 else 'dlc', app))
        else:
            kinds.append((g, by_name(g), None))
    apps = [a for _, k, a in kinds if k == 'base' and a]
    app = next((a for a in apps if any(k == 'update' and x == a for _, k, x in kinds)), apps[0] if apps else None)
    for g, kind, a in kinds:
        if kind and not found[kind] and (a is None or app is None or a == app):
            found[kind] = g
    missing = [what for what, key in (('the base game', 'base'), ('the update', 'update'), ('prod.keys', 'keys'),
                                      ('the firmware folder (.nca files)', 'firmware')) if not found[key]]
    if missing:
        raise GameFolderError(f'{folder} lacks {", ".join(missing)}')
    return found
