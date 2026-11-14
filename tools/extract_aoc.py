"""Extract the add-on content (DLC) of Scarlet or Violet from the player's own DLC NSP files.

Each NSP holds a data NCA with a small RomFS, the title's add-on content (for Scarlet and Violet only keyfile.dat, the
licence for The Teal Mask and The Indigo Disk, whose data ships in the 4.0.0 update). Its files go to
<work>/extracted/aoc/<title id>/, where runtime/ov_system.c (nn::aoc) and runtime/fs_host.c MountAddOnContent find
them; the installer copies that folder next to totk.exe as aoc/. The decryption key comes from the NSP's own common ticket and the keys file the
player names; no key leaves the files it is read from.

Usage: python tools/extract_aoc.py <dlc.nsp> [...] [--keys prod.keys] [--work build]
"""
import argparse
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools' / 'vendor' / 'macnativegames'))
from extract_exefs import common_tickets, container_entries, load_keys, nca_inventory, require  # noqa: E402
from extract_romfs import index_romfs  # noqa: E402
from extract_shared_fonts import Slice, nca_romfs  # noqa: E402

AOC_CONTENT = (4, 5)      # NCA content types of add-on content: Data, PublicData
SV_AOC_BASES = {'0100A3D008C5C000': 'Scarlet', '01008F6008C5E000': 'Violet'}


def aoc_owner(title_id):
    """The game an add-on content title belongs to: its title ID with the low 12 bits cleared, less 0x1000."""
    base = f'{(int(title_id, 16) & ~0xFFF) - 0x1000:016X}'
    return base, SV_AOC_BASES.get(base)


def extract(nsp, keys, out_root):
    """Extract every add-on content NCA of nsp; returns [(title id, game, index entries)]."""
    done = []
    with nsp.open('rb') as f:
        kind, entries = container_entries(f, nsp.stat().st_size)
        require(kind == 'nsp', f'{nsp} is not an NSP')
        tickets = common_tickets(f, entries)
        records = [(r, h) for r, h in nca_inventory(f, entries, keys) if r['content_type'] in AOC_CONTENT]
    require(records, f'{nsp} holds no add-on content')
    for record, _ in records:
        title = record['title_id']
        base, game = aoc_owner(title)
        out = out_root / title
        out.mkdir(parents=True, exist_ok=True)
        image = out.with_name(title + '.romfs.tmp')
        nca_romfs(Slice(nsp, record['nca_offset']), keys, image, tickets)
        files = index_romfs(image)
        with image.open('rb') as f:
            for entry in files:
                target = out / entry['path']
                require(target.resolve().is_relative_to(out.resolve()), f'Unsafe path {entry["path"]}')
                target.parent.mkdir(parents=True, exist_ok=True)
                f.seek(entry['offset'])
                target.write_bytes(f.read(entry['size']))
        image.unlink()
        done.append((title, game or f'title {base}', files))
    return done


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('nsp', type=Path, nargs='+', help='DLC NSP files')
    ap.add_argument('--keys', type=Path, help='prod.keys (default: input.keys of config.local.toml)')
    ap.add_argument('--work', type=Path, help='output directory with extracted/ (default: work.dir of the config, '
                                              'else build)')
    args = ap.parse_args()
    cfg_path = ROOT / 'config.local.toml'
    cfg = tomllib.loads(cfg_path.read_text(encoding='utf-8')) if cfg_path.is_file() else {}
    keys_path = args.keys or Path(cfg.get('input', {}).get('keys', ''))
    if not keys_path.is_file():
        sys.exit('No keys file: pass --keys or set input.keys in config.local.toml')
    work = args.work or ROOT / cfg.get('work', {}).get('dir', 'build')
    keys = load_keys(keys_path)
    for nsp in args.nsp:
        for title, game, files in extract(nsp, keys, work / 'extracted' / 'aoc'):
            print(f'{nsp.name}: add-on content {title} of {game}, {len(files)} files')


if __name__ == '__main__':
    main()
