"""Survey the ExeFS of Scarlet/Violet and compare its OS surface with what the TotK runtime already provides.

Reads build/extracted/<ver>/exefs (main, sdk, subsdk*, main.npdm) and writes build/analysis/<ver>/:

- modules.json: per module build id, segment sizes, needed libraries, symbol counts, SDK version strings;
- imports.txt: every symbol a module imports from another one, with the module that defines it;
- runtime_coverage.txt: the nn:: functions the game calls that the runtime replaces (host overrides), and the ones it
  does not, grouped by nn:: namespace;
- services.txt: the IPC services main.npdm allows the game to open.

Nothing it writes may be committed: the output describes game code and stays under build/.
Usage: python tools/sv_analyze.py [--version v400]
"""
import argparse
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'recomp'))
from nso import Nso  # noqa: E402

STB_GLOBAL, STB_WEAK = 1, 2
SDK_TAG = re.compile(rb'(?:SDK MW\+|NintendoSdk_)[\x20-\x7e]{4,120}')
MANGLED_LITERAL = re.compile(r'"(_Z[A-Za-z0-9_]+)"')


def demangle_ns(name):
    """nn::<ns> for an Itanium-mangled name in namespace nn, else the first namespace or '(other)'."""
    m = re.match(r'_ZN(?:K|VK)?(\d+)([A-Za-z_]\w*)', name)
    if not m:
        return '(other)'
    n, rest = int(m.group(1)), m.group(2)
    first = rest[:n]
    tail = rest[n:]
    if first != 'nn':
        return first
    m2 = re.match(r'(\d+)([A-Za-z_]\w*)', tail)
    if not m2:
        return 'nn'
    k = int(m2.group(1))
    return 'nn::' + m2.group(2)[:k]


def runtime_overrides():
    names = set()
    for p in (ROOT / 'runtime').glob('*.c'):
        names.update(MANGLED_LITERAL.findall(p.read_text(encoding='utf-8', errors='replace')))
    return names


def npdm_services(path):
    raw = path.read_bytes()
    out = set()
    i = raw.find(b'ACI0')
    if i < 0:
        return []
    sac_off, sac_size = struct.unpack_from('<II', raw, i + 0x28)
    sac = raw[i + sac_off:i + sac_off + sac_size]
    j = 0
    while j < len(sac):
        ctrl = sac[j]
        n = (ctrl & 0x7) + 1
        is_server = bool(ctrl & 0x80)
        name = sac[j + 1:j + 1 + n].decode('ascii', 'replace')
        if not is_server:
            out.add(name)
        j += 1 + n
    return sorted(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', default='v400')
    args = ap.parse_args()
    exefs = ROOT / 'build' / 'extracted' / args.version / 'exefs'
    out = ROOT / 'build' / 'analysis' / args.version
    out.mkdir(parents=True, exist_ok=True)

    names = ['main', 'sdk'] + sorted(p.name for p in exefs.glob('subsdk*'))
    mods, info = {}, {}
    for n in names:
        m = Nso(str(exefs / n))
        mods[n] = m
        defined = {s[0] for s in m.syms if s[0] and s[2] != 0}
        undefined = {s[0] for s in m.syms if s[0] and s[2] == 0}
        tags = sorted({t.decode() for t in SDK_TAG.findall(bytes(m.image))})
        info[n] = {
            'build_id': m.build_id,
            'text': m.text[1], 'rodata': m.rodata[1], 'data': m.data[1], 'bss': m.bss_size,
            'needed': m.needed,
            'defined_symbols': len(defined), 'imported_symbols': len(undefined),
            'sdk_tags': tags[:10],
            'relocs': len(getattr(m, 'relocs', [])),
        }
    (out / 'modules.json').write_text(json.dumps(info, indent=2))

    owner = {}
    for n, m in mods.items():
        for s in m.syms:
            if s[0] and s[2] != 0:
                owner.setdefault(s[0], n)
    imports = defaultdict(list)
    for n, m in mods.items():
        for s in m.syms:
            if s[0] and s[2] == 0:
                imports[n].append((s[0], owner.get(s[0], '(unresolved)')))
    with open(out / 'imports.txt', 'w') as f:
        for n in names:
            for sym, o in sorted(imports[n]):
                f.write(f'{n}\t{o}\t{sym}\n')

    # Everything main and subsdk* import from sdk is the game's OS surface (the SDK talks to Horizon through IPC/SVCs).
    surface = {sym for n in names if n != 'sdk' for sym, o in imports[n] if o == 'sdk'}
    nn_surface = {s for s in surface if s.startswith('_ZN2nn') or s.startswith('_ZNK2nn')}
    ovr = runtime_overrides()
    covered = nn_surface & ovr
    missing = nn_surface - ovr
    by_ns_missing = Counter(demangle_ns(s) for s in missing)
    by_ns_covered = Counter(demangle_ns(s) for s in covered)
    # Overrides the TotK runtime has that Scarlet's sdk does not even define: renamed or new SDK functions.
    sdk_defined = {s[0] for s in mods['sdk'].syms if s[0] and s[2] != 0}
    stale = sorted(o for o in ovr if o.startswith('_ZN') and o not in sdk_defined
                   and not any(o in {s[0] for s in m.syms} for m in mods.values()))
    with open(out / 'runtime_coverage.txt', 'w') as f:
        f.write(f'game -> sdk imports: {len(surface)}; nn:: {len(nn_surface)}; '
                f'replaced by runtime {len(covered)}; not replaced {len(missing)}\n\n')
        f.write('namespace\treplaced\tnot_replaced\n')
        for ns in sorted(set(by_ns_missing) | set(by_ns_covered), key=lambda k: -(by_ns_missing[k] + by_ns_covered[k])):
            f.write(f'{ns}\t{by_ns_covered[ns]}\t{by_ns_missing[ns]}\n')
        f.write('\n# not replaced\n')
        for s in sorted(missing):
            f.write(s + '\n')
        f.write(f'\n# runtime overrides with no symbol in this game ({len(stale)})\n')
        for s in stale:
            f.write(s + '\n')

    services = npdm_services(exefs / 'main.npdm')
    (out / 'services.txt').write_text('\n'.join(services) + '\n')

    print(f'modules: {", ".join(names)}')
    for n in names:
        i = info[n]
        print(f'  {n:8} text {i["text"]/2**20:6.1f} MiB  rodata {i["rodata"]/2**20:5.1f}  data {i["data"]/2**20:5.1f}'
              f'  bss {i["bss"]/2**20:6.1f}  syms def {i["defined_symbols"]:6} imp {i["imported_symbols"]:5}'
              f'  needed {i["needed"]}')
    print(f'sdk tags: {info["sdk"]["sdk_tags"][:3]}')
    print(f'OS surface: {len(surface)} sdk imports, {len(nn_surface)} nn::; '
          f'runtime replaces {len(covered)}, not {len(missing)}; stale overrides {len(stale)} of {len(ovr)}')
    print('top namespaces not replaced:', ', '.join(f'{k} {v}' for k, v in by_ns_missing.most_common(15)))
    print(f'services in npdm: {len(services)}')
    print(f'written to {out.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
