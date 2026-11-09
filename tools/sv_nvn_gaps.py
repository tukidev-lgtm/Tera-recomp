"""List the NVN procs a game's main asks for by name and which of them the runtime does not implement.

The game's proc loader calls nvnDeviceGetProcAddress with every name it uses; those names are NUL-terminated strings
in main's .rodata. runtime/nvn_procs.inc lists the names the runtime registers; a proc counts as implemented when
some runtime/*.c defines nvnp_<name> outside nvn_procs.c (whose definitions are weak stubs that stop the game).

    python tools/sv_nvn_gaps.py [--version v400]

Writes build/analysis/<version>/nvn_gaps.txt (ignored by git) and prints the counts.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'recomp'))
from nso import Nso  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', default='v400')
    args = ap.parse_args()
    m = Nso(str(ROOT / 'build' / 'extracted' / args.version / 'exefs' / 'main'))
    ro_off, ro_len = m.rodata
    ro = bytes(m.image[ro_off:ro_off + ro_len])
    wanted = sorted({s.decode() for s in re.findall(rb'(?<=\x00)(nvn[A-Z][A-Za-z0-9]+)(?=\x00)', ro)})
    registered = set(re.findall(r'NVN_PROC\((\w+)\)', (ROOT / 'runtime' / 'nvn_procs.inc').read_text()))
    implemented = set()
    for p in (ROOT / 'runtime').glob('*.c'):
        if p.name == 'nvn_procs.c' or p.name.endswith('_probe.c'):
            continue
        implemented.update(re.findall(r"\b(?:NVN_API\(|void\s+nvnp_)(\w+)", p.read_text(encoding="utf-8", errors="replace")))
    implemented.add('nvnDeviceGetProcAddress')
    unknown = [n for n in wanted if n not in registered]
    stub = [n for n in wanted if n in registered and n not in implemented]
    out = ROOT / 'build' / 'analysis' / args.version
    out.mkdir(parents=True, exist_ok=True)
    with open(out / 'nvn_gaps.txt', 'w') as f:
        f.write(f'# asked {len(wanted)}, implemented {len(wanted) - len(unknown) - len(stub)}, '
                f'registered but stubbed {len(stub)}, not registered {len(unknown)}\n')
        f.write('\n# registered, stub\n' + ''.join(n + '\n' for n in stub))
        f.write('\n# not registered (nvnDeviceGetProcAddress returns NULL)\n' + ''.join(n + '\n' for n in unknown))
    print(f'asked {len(wanted)}; implemented {len(wanted) - len(unknown) - len(stub)}; stub {len(stub)}; '
          f'not registered {len(unknown)} -> {(out / "nvn_gaps.txt").relative_to(ROOT)}')


if __name__ == '__main__':
    main()
