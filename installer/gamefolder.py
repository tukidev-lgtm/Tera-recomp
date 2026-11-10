"""--game <folder>: find the setup's inputs in one folder the player points to (RecompLauncher passes it as {game}).

The folder holds the player's own files, at its top or in subfolders up to MAX_DEPTH deep:
    the base game and the update     .nsp, .xci, .nsz or .xcz; told apart by the title ID of their Meta NCA (an update's
                                     ends in 800), read with the keys, else by the name ("UPD", "[v0]")
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
    """{'base', 'update', 'keys', 'firmware'} paths found in folder; raises GameFolderError naming what is missing."""
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
    found = {'keys': keyfile, 'firmware': firmware, 'base': None, 'update': None}
    loaded = None
    if keyfile:
        from extract_exefs import load_keys
        try:
            loaded = load_keys(keyfile)
        except (OSError, ValueError, UnicodeDecodeError):
            loaded = None
    for g in games:
        title = meta_title(g, loaded) if loaded else None
        kind = ('update' if title.endswith('800') else 'base') if title else by_name(g)
        if kind and not found[kind]:
            found[kind] = g
    missing = [what for what, key in (('the base game', 'base'), ('the update', 'update'), ('prod.keys', 'keys'),
                                      ('the firmware folder (.nca files)', 'firmware')) if not found[key]]
    if missing:
        raise GameFolderError(f'{folder} lacks {", ".join(missing)}')
    return found
