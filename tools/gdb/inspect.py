"""GDB (gdb-multiarch with Python) helper: read Scarlet's Wwise singleton and its registry from a running game in an
emulator's GDB stub, then detach. No breakpoints: the objects persist, so this runs any time after boot (title screen).

Chain read: GOT slot main+0x46d8288 -> global -> singleton -> singleton+0x58 (the registry the port never creates,
docs/phase3-boot.md) -> the registry's vtable, which names its class; a few levels of the registry are dumped too.
Output also goes to inspect.log in gdb's working directory.

    C:\\msys64\\ucrt64\\bin\\gdb-multiarch.exe -x tools/gdb/inspect.gdb
"""
import re

import gdb

LOG = open("inspect.log", "a")


def say(text):
    print(text)
    LOG.write(text + "\n")
    LOG.flush()


def find_main():
    info = gdb.execute("monitor get info", to_string=True)
    for line in info.splitlines():
        if re.search(r"\bmain(_app)?(\.nss|\.nso)?\s*$", line.strip()):
            return int(re.search(r"0x[0-9a-fA-F]+", line).group(0), 16)
    raise gdb.GdbError("inspect: no main module in `monitor get info`")


MAIN = find_main()


def u64(a):
    try:
        return int(gdb.parse_and_eval(f"*(unsigned long long *){a:#x}")) & (2**64 - 1)
    except gdb.error:
        return None


def where(a):
    if a is None:
        return "?"
    return f"main+{a - MAIN:#x}" if MAIN <= a < MAIN + 0x5000000 else f"{a:#x}"


def dump(label, a, n=16):
    if not a:
        say(f"{label}: null")
        return []
    words = [u64(a + 8 * i) for i in range(n)]
    if words[0] is None:
        say(f"{label} {a:#x}: unreadable")
        return []
    say(f"{label} {a:#x}: vtable {where(words[0])} | " +
        " ".join(f"+{8 * i:02x} {where(w) if w and MAIN <= w < MAIN + 0x5000000 else hex(w)}"
                 for i, w in enumerate(words[1:], 1) if w is not None))
    return words


say(f"inspect: main {MAIN:#x}")
glob = u64(MAIN + 0x46d8288)
say(f"GOT main+0x46d8288 -> global {glob:#x}" if glob else "GOT slot unreadable")
single = u64(glob) if glob else None
words = dump("singleton", single)
reg = words[11] if len(words) > 11 else None   # +0x58
say(f"singleton+0x58 = {reg:#x}" if reg is not None else "singleton+0x58: ?")
rw = dump("registry", reg)
for i, w in enumerate(rw[1:8], 1):   # one level into the registry's pointer fields
    if w and w > 0x100000 and not (MAIN <= w < MAIN + 0x5000000):
        dump(f"  registry+{8 * i:#x} ->", w, 8)
say("inspect: done")
