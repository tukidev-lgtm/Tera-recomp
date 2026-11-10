"""Command line: python -m installer --base FILE --update FILE --keys FILE --lang CODE --dest DIR [--firmware DIR]
                                     [--root DIR] [--temp DIR] [--verify] [--dlss]

A first install needs every option. --firmware names the Switch firmware folder (an unpacked firmware or an emulator's
Contents/registered); the setup extracts the system shared fonts from it when the language is Chinese, Korean or
Traditional Chinese. On a folder with totk-install.json (resume after a cancel or failure, or update with a newer
release) only --dest is needed: --lang defaults to the recorded language, and the game files are needed only when the
extraction has to run again. Prints JSON events on stdout (events.py). Exit code: 0 done, 1 failed, 2 bad command
line, 3 cancelled (Ctrl+C or Ctrl+Break).

--dlss downloads NVIDIA's DLSS library (nvngx_dlss.dll) from NVIDIA's DLSS SDK repository into the install folder on
Windows with an NVIDIA RTX GPU (steps.py, Dlss).

--game FOLDER finds the base game, the update, prod.keys and the firmware folder in one folder of the player's
(installer/gamefolder.py); options given explicitly win. --lang auto leaves the language to the game, which follows the
Windows user locale. --progress-lines prints `progress <0..1> <text>` lines instead of the JSON events, for
RecompLauncher's windowless setup step (recomplauncher.json "prepare"); the log file is the same.
"""
import argparse
import hashlib
import signal
import sys
import time
from pathlib import Path

from . import layout as layouts
from . import state as statefile
from .events import Reporter, render
from .runner import Context, run_steps
from .steps import all_steps, migrate_state

EXIT = {'ok': 0, 'failed': 1, 'cancelled': 3}


class LineOut:
    """stdout for --progress-lines: each JSON event becomes `progress <fraction> <text>` (the whole setup as 0..1, the
    current step and phase as text), and errors become `error <text>`. Other events print nothing."""

    def __init__(self, out=sys.stdout):
        self.out, self.steps, self.index, self.reporter = out, [], 0, None

    def write(self, line):
        import json
        try:
            e = json.loads(line)
        except ValueError:
            return
        c = self.reporter.catalog if self.reporter else {}
        if e['type'] == 'start':
            self.steps = e.get('steps', [])
        elif e['type'] == 'step' and e['step'] in self.steps:
            self.index = self.steps.index(e['step']) + (e['state'] != 'started')
            self.line(0, render(c, 'step.' + e['step']))
        elif e['type'] == 'progress' and e['step'] in self.steps:
            self.index = self.steps.index(e['step'])
            part = e['done'] / e['total'] if e.get('total') else 0
            self.line(min(part, 1), render(c, f'phase.{e["step"]}.{e["phase"]}'))
        elif e['type'] == 'error':
            self.out.write(f'error {render(c, e["id"], e.get("params"))}\n')
            self.out.flush()

    def line(self, part, text):
        whole = (self.index + part) / len(self.steps) if self.steps else 0
        self.out.write(f'progress {min(whole, 1):.3f} {text}\n')
        self.out.flush()

    def flush(self):
        self.out.flush()


class Parser(argparse.ArgumentParser):
    def error(self, message):
        Reporter().error('cli.usage', {'detail': message})
        sys.exit(2)


# The temp folder inside the install folder when the layout names none (the Linux release); the check step does not
# count it when it requires an empty install folder
TEMP_IN_DEST = '.setup-temp'


def temp_folder(base, dest):
    """One temp folder per install folder, so two installs never share generated files."""
    tag = hashlib.sha256(str(dest).lower().encode()).hexdigest()[:8]
    return base / f'{dest.name or "root"}-{tag}'


def cancel(signum, frame):
    raise KeyboardInterrupt


def main(argv=None):
    ap = Parser(prog='python -m installer', description=__doc__.splitlines()[0])
    ap.add_argument('--base', type=Path, help='base game, XCI or NSP (or NSZ, XCZ: unpacked first)')
    ap.add_argument('--update', type=Path, help='the update NSP or NSZ: Pokemon Scarlet 4.0.0')
    ap.add_argument('--keys', type=Path, help='prod.keys')
    ap.add_argument('--firmware', type=Path, help='Switch firmware folder (Contents/registered): the shared fonts '
                                                   'Pokemon Scarlet draws its text with')
    ap.add_argument('--lang', help='game language, a Switch language code (en-US, en-GB, ja, de, ru, ...)')
    ap.add_argument('--dest', type=Path, required=True, help='install folder')
    ap.add_argument('--root', type=Path, help='release folder or repository (default: the folder above installer/)')
    ap.add_argument('--temp', type=Path, help='temp folder (default: temp/ of the release on Windows, .setup-temp in '
                                              'the install folder on Linux, build/install-temp in a repository)')
    ap.add_argument('--verify', action='store_true', help='hash every kept file, also those this release made')
    ap.add_argument('--dlss', action='store_true', help="download NVIDIA's nvngx_dlss.dll (NVIDIA RTX SDK license) next "
                                                      'to the game, for dlaa and the DLSS modes')
    ap.add_argument('--game', type=Path, help="a folder holding the base game, the update, prod.keys and the firmware "
                                              "(installer/gamefolder.py); explicit options win")
    ap.add_argument('--progress-lines', action='store_true', help='print `progress <0..1> <text>` lines instead of '
                                                                  'JSON events (RecompLauncher)')
    ap.add_argument('--no-shortcut', dest='shortcut', action='store_false',
                    help='no desktop shortcut to the game (Windows; made once, on the first finished install)')
    args = ap.parse_args(argv)
    lines = LineOut() if args.progress_lines else None
    if args.game:
        from . import steps as _steps   # noqa: F401  (puts tools/ and the vendored extractors on sys.path)
        from .gamefolder import GameFolderError, find
        try:
            found = find(args.game)
        except GameFolderError as e:
            rep = Reporter(out=lines)
            if lines:
                lines.reporter = rep
            rep.error('cli.game_folder', {'detail': str(e)})
            return 1
        for name in ('base', 'update', 'keys', 'firmware'):
            if not getattr(args, name):
                setattr(args, name, found[name])

    # The window cancels with Ctrl+Break to the installer's process group (CREATE_NEW_PROCESS_GROUP), which also reaches
    # the tools it runs; Python's default for it ends the process without recording anything.
    for name in ('SIGBREAK', 'SIGTERM'):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), cancel)
    layout = layouts.find(args.root)
    dest = args.dest.resolve()
    base = args.temp or layout.temp
    temp = temp_folder(base.resolve(), dest) if base else dest / TEMP_IN_DEST
    log = temp / 'logs' / time.strftime('install-%Y%m%d-%H%M%S.log')
    rep = Reporter(out=lines, log_path=log)
    if lines:
        lines.reporter = rep
    t0 = time.monotonic()
    try:
        state = statefile.load(dest)
    except statefile.StateError as e:
        rep.error(e.id, {'path': str(e.path)})
        rep.emit('end', result='failed', seconds=0)
        return 1
    ctx = Context(layout, dest, temp, rep, args, state=state, verify=args.verify)
    migrate_state(ctx)
    steps = all_steps()
    rep.emit('start', release=layout.release, mode=ctx.mode, dev=layout.dev, dest=str(dest), log=str(log),
             steps=[s.id for s in steps])
    result = run_steps(steps, ctx)
    rep.emit('end', result=result, seconds=round(time.monotonic() - t0, 1))
    rep.close()
    return EXIT[result]
