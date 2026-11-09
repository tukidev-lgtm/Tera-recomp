"""GDB (gdb-multiarch with Python) helper: log registers, memory and the call chain at code addresses of a game
running in an emulator's GDB stub, without stopping the game. Uses software breakpoints only, so it works on stubs
without watchpoints (Ryujinx Canary 1.3.109+).

Built for Scarlet 4.0.0's boot problem (docs/phase3-boot.md): on the port, main+0x14ff4dc finds the registry at the
Wwise singleton's +0x58 empty. On an emulator, where the game works, the default probe there reports the registry's
address and vtable, which names its class.

Usage: gdb-multiarch -x tools/gdb/scarlet.gdb (connects to localhost:22225 and sources this), or inside gdb connected
to the emulator: `source tools/gdb/probe.py` then `continue`. main's base is read from `monitor get info`; set $main
first to override it.
More probes, before `source`: set $probes = "14ff4dc,1234abc" (main offsets, hex). Each probe reports its first
$probe_limit hits (default 4). Everything is also written to probe.log in gdb's working directory.
"""
import re

import gdb


def find_main():
    v = gdb.parse_and_eval("$main")
    if str(v.type) != "void":
        return int(v)
    info = gdb.execute("monitor get info", to_string=True)
    for line in info.splitlines():
        if re.search(r"\bmain(_app)?(\.nss|\.nso)?\s*$", line.strip()):   # Ryujinx names it main_app.nss
            m = re.search(r"0x[0-9a-fA-F]+", line)
            if m:
                return int(m.group(0), 16)
    print(info)
    raise gdb.GdbError("probe: no 'main' module in `monitor get info`; set $main = <base> and source again")


MAIN = find_main()


def conv(name):
    v = gdb.parse_and_eval(name)
    return None if str(v.type) == "void" else v


LIMIT = int(conv("$probe_limit") or 4)
_p = conv("$probes")
PROBES = [int(x, 16) for x in (_p.string() if _p is not None else "14ff4dc").split(",") if x.strip()]
LOG = open("probe.log", "a")


def say(text):
    print(text)
    LOG.write(text + "\n")
    LOG.flush()


def u64(a):
    try:
        return int(gdb.parse_and_eval(f"*(unsigned long long *){a:#x}")) & (2**64 - 1)
    except gdb.error:
        return None


def reg(r):
    return int(gdb.parse_and_eval(f"${r}")) & (2**64 - 1)


def where(a):
    if a is None:
        return "?"
    return f"main+{a - MAIN:#x}" if MAIN <= a < MAIN + 0x5000000 else f"{a:#x}"


def dump(label, a, n=16):
    if not a:
        say(f"    {label}: null")
        return
    words = [u64(a + 8 * i) for i in range(n)]
    if words[0] is None:
        say(f"    {label} {a:#x}: unreadable")
        return
    say(f"    {label} {a:#x}: vtable {where(words[0])} | " +
        " ".join(f"+{8 * i:02x} {w:#x}" for i, w in enumerate(words[1:], 1) if w is not None))


def chain(n=12):
    out, fp = [where(reg("x30"))], reg("x29")
    for _ in range(n):
        if not fp or fp & 7:
            break
        nxt, lr = u64(fp), u64(fp + 8)
        if lr is None:
            break
        out.append(where(lr))
        if not nxt or nxt <= fp:
            break
        fp = nxt
    return " <- ".join(out)


class Probe(gdb.Breakpoint):
    def __init__(self, off):
        super().__init__(f"*{MAIN + off:#x}", internal=False)
        self.off, self.hits = off, 0

    def stop(self):
        self.hits += 1
        if self.hits > LIMIT:
            self.enabled = False
            return False
        x = [reg(f"x{i}") for i in range(31)]
        say(f"probe main+{self.off:#x} #{self.hits}: " + " ".join(f"x{i} {v:#x}" for i, v in enumerate(x[:9])) +
            f" | x19 {x[19]:#x} x20 {x[20]:#x} x21 {x[21]:#x}")
        say(f"    calls: {chain()}")
        dump("[x0]", x[0])
        if x[0]:
            reg58 = u64(x[0] + 0x58)
            say(f"    [x0]+0x58 = {reg58:#x}" if reg58 is not None else "    [x0]+0x58 unreadable")
            if reg58:
                dump("[[x0]+0x58]", reg58)
        return False   # keep the game running


say(f"probe: main {MAIN:#x}, probes {', '.join(f'main+{o:#x}' for o in PROBES)}, {LIMIT} hits each")
for off in PROBES:
    Probe(off)
say("probe: armed; type `continue` if the game is paused")
