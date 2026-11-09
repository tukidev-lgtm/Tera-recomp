"""Pack a RomFS extracted as a folder into the romfs.bin + index.json pair the runtime reads.

TotK Recomp's tools/extract.py keeps the RomFS as the original image (romfs.bin) and lists its files in index.json;
runtime/romfs.c serves file reads as offsets into that image. A RomFS dumped by another tool is a folder tree. This
concatenates its files into one image, each aligned to 16 bytes, and writes the index.json tools/romfs_index.py and
tools/build_game.py read: {status, romfs_size, romfs_sha256, files: [{path, offset, size}]}, paths relative to the
RomFS root with '/' separators.

    .venv\\Scripts\\python tools\\pack_romfs.py --src build\\extracted\\v400\\romfs --out build\\extracted\\v400\\romfs-image

Needs as much free space as the RomFS. Standard library only. The output is game data and stays under build/.
"""
import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

ALIGN = 16
BUF = 8 << 20


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--src', type=Path, required=True, help='RomFS folder')
    ap.add_argument('--out', type=Path, required=True, help='directory for romfs.bin and index.json')
    args = ap.parse_args()
    src, out = args.src.resolve(), args.out.resolve()
    if not src.is_dir():
        sys.exit(f'pack_romfs: {src} is not a folder')
    if out == src or src in out.parents:
        sys.exit('pack_romfs: --out must be outside --src')
    files = sorted((p for p in src.rglob('*') if p.is_file()), key=lambda p: p.relative_to(src).as_posix().encode())
    total = sum(p.stat().st_size for p in files)
    out.mkdir(parents=True, exist_ok=True)
    image = out / 'romfs.bin'
    tmp = out / 'romfs.bin.part'
    print(f'pack_romfs: {len(files)} files, {total / 2**30:.2f} GiB -> {image}', flush=True)
    h = hashlib.sha256()
    entries = []
    pos = 0
    t0 = last = time.monotonic()
    with tmp.open('wb') as f:
        for p in files:
            pad = (-pos) % ALIGN
            if pad:
                z = b'\0' * pad
                f.write(z)
                h.update(z)
                pos += pad
            size = 0
            with p.open('rb') as r:
                while chunk := r.read(BUF):
                    f.write(chunk)
                    h.update(chunk)
                    size += len(chunk)
            entries.append({'path': p.relative_to(src).as_posix(), 'offset': pos, 'size': size})
            pos += size
            now = time.monotonic()
            if now - last > 10:
                print(f'  {pos / 2**30:6.2f} / {total / 2**30:.2f} GiB', flush=True)
                last = now
    os.replace(tmp, image)
    manifest = {'status': 'pass', 'source': 'folder', 'romfs_size': pos, 'romfs_sha256': h.hexdigest(),
                'files': entries}
    (out / 'index.json').write_text(json.dumps(manifest, indent=1))
    print(f'pack_romfs: done in {time.monotonic() - t0:.0f} s, {pos} bytes, sha256 {h.hexdigest()}')


if __name__ == '__main__':
    main()
