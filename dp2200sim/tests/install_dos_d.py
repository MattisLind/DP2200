"""Install DOS.D 2.6 on a new 9374 pack and verify a fresh-process disk boot."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import tempfile

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]
MEDIA = ROOT / 'DOS.D'
IMAGE_BYTES = 204 * 8 * 24 * 256


class Session:
    def __init__(self, sim, output, stage):
        self.sim, self.output, self.stage = sim, output, stage
        self.state = sim.command('state')
        self.history = []

    def run(self, count=1000000):
        self.state = self.sim.command(f'run {count}', timeout=60)
        if not self.history or self.history[-1]['screen'] != self.state['screen']:
            self.history.append({k: self.state[k] for k in ('instructions', 'pc', 'halted', 'screen')})
            (self.output / f'{self.stage}.json').write_text(json.dumps(self.history, indent=2) + '\n')
            (self.output / f'{self.stage}.screen.txt').write_text(self.state['screen'])
        return self.state

    def until(self, text, budget=500000000, after=None):
        for _ in range(budget // 1000000):
            self.run()
            screen = self.state['screen']
            if text in screen and (after is None or screen.rfind(text) > screen.rfind(after) >= 0):
                return self.state
            if self.state['halted']:
                break
        raise AssertionError(f'{self.stage}: did not reach {text!r}:\n{self.state["screen"]}\n'
                             f'{self.sim.command("inspect")["debug"]}')

    def keys(self, text):
        for character in text:
            for _ in range(20):
                if not self.state['keyboard_ready']:
                    break
                self.run()
            if self.state['keyboard_ready']:
                raise AssertionError('Keyboard latch was not consumed')
            self.sim.key(character)
            self.run(100000)

    def answer(self, prompt, text):
        self.until(prompt)
        self.keys(text + '\n')

    def finish(self):
        stats = self.sim.command('disk-state')['disk']
        if stats['errors']:
            raise AssertionError(f'{self.stage}: disk errors: {stats}')
        (self.output / f'{self.stage}.disk.json').write_text(json.dumps(stats, indent=2) + '\n')


def machine(output):
    sim = Harness()
    try:
        sim.command('cpu 5500')
        sim.command(f'pc {0o170036}')
        state = sim.command('run 10000000', timeout=60)
        if not state['halted']:
            raise AssertionError('Power-up ROM did not finish')
        sim.command('disk-model 9374')
        sim.command(f'disk 0 0 {output / "removable.dsk"}')
        sim.command(f'disk 1 1 {output / "fixed.dsk"}')
        return sim
    except Exception:
        sim.__exit__()
        raise


def boot(output, all_drives=False):
    output = Path(output).resolve()
    for name in ('removable.dsk', 'fixed.dsk', 'boot.tap'):
        if not (output / name).is_file():
            raise FileNotFoundError(output / name)
    with machine(output) as sim:
        sim.load(output / 'boot.tap')
        session = Session(sim, output, 'cold-boot')
        session.until('READY', budget=30000000)
        session.keys('CAT :D0\n')
        session.until('DIRECTORY CONTENTS COMMAND', budget=10000000)
        session.until('READY', after='SYSTEM0/SYS', budget=10000000)
        for name in ('SYSTEM0/SYS', 'MIN/CMD', 'UTILITY/SYS', 'DOSD/RFM'):
            if not any(name in frame['screen'] for frame in session.history):
                raise AssertionError(f'Missing installed file {name}')
        if all_drives:
            session.keys('CAT\n')
            session.until('READY', after='DRIVE  1', budget=10000000)
            session.run()
            if session.state['beeps']:
                raise AssertionError('Catalogue caused a bell')
            session.keys('HELP\n')
            session.until('WHAT?', budget=10000000)
            session.run()
            if session.state['beeps']:
                raise AssertionError('Unknown command caused a repeating bell')
        if any('FAILURE IN SYSTEM DATA' in frame['screen'] for frame in session.history):
            raise AssertionError('CAT reported corrupt system data')
        session.finish()
    result = 'Fresh 5500 process booted the installed disk and catalogued its files'
    return result


def initialize_fixed(output):
    """DOSGEN the fixed pack from the installed removable volume."""
    output = Path(output).resolve()
    with machine(output) as sim:
        # Reopen for writes: the normal boot helper attaches the companion read-only.
        sim.command(f'disk 1 0 {output / "fixed.dsk"}')
        sim.load(output / 'boot.tap')
        session = Session(sim, output, 'fixed-generation')
        session.until('READY')
        session.keys('DOSGEN :D1\n')
        session.answer('STARTS BY ERASING', 'Y')
        session.answer('DO YOU MIND LOSING', 'N')
        session.answer('ARE YOU SURE?', 'Y')
        session.answer('LOCK OUT ANY CYLINDERS', 'N')
        session.until('THE SECTORS HAVE BEEN SUCCESSFULLY WRITTEN')
        session.until('READY', after='THE SECTORS HAVE BEEN SUCCESSFULLY WRITTEN')
        session.finish()
    return 'Fixed pack DOSGEN completed, including system files and PUTIPL'


def repair_fixed(output):
    """Validate a generated replacement on copies and preserve the original pack."""
    output = Path(output).resolve()
    backup = output / 'fixed.dsk.before-dosgen'
    if backup.exists():
        raise FileExistsError(f'Preserve the existing backup before rerunning: {backup}')
    with tempfile.TemporaryDirectory(prefix='dos-fixed-') as directory:
        work = Path(directory)
        for name in ('removable.dsk', 'fixed.dsk', 'boot.tap'):
            shutil.copy2(output / name, work / name)
        message = initialize_fixed(work)
        boot(work, all_drives=True)
        with backup.open('xb') as saved, (output / 'fixed.dsk').open('rb') as original:
            shutil.copyfileobj(original, saved)
        # Replace only the fixed pack after the fresh-process verification passes.
        with tempfile.NamedTemporaryFile(dir=output, prefix='.fixed-', delete=False) as staged:
            replacement = Path(staged.name)
            with (work / 'fixed.dsk').open('rb') as generated:
                shutil.copyfileobj(generated, staged)
        shutil.copymode(output / 'fixed.dsk', replacement)
        os.replace(replacement, output / 'fixed.dsk')
        for report in work.glob('fixed-generation.*'):
            shutil.copy2(report, output / report.name)
    return message + '; fresh boot and both-drive CAT passed; backup: ' + str(backup)


def install(output):
    output = Path(output).resolve()
    tapes = {n: MEDIA / f'dos.d_2.6_7_MAR_81_{n}of5.tap' for n in range(1, 6)}
    for path in tapes.values():
        if not path.is_file():
            raise FileNotFoundError(path)
    # Each installation owns a new directory; never replace an existing image.
    output.mkdir(parents=True, exist_ok=False)
    for name in ('removable.dsk', 'fixed.dsk'):
        with (output / name).open('xb') as image:
            image.truncate(IMAGE_BYTES)
    checks = []

    def passed(message):
        checks.append(message)
        (output / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n')
        print(message, flush=True)

    print(f'Installation directory: {output}', flush=True)
    with machine(output) as sim:
        sim.load(tapes[1])
        session = Session(sim, output, 'format')
        session.answer('TYPE IN THE PHYSICAL DRIVE', '0')
        session.answer('ARE YOU SURE THE WRITE PROTECT SWITCHES', 'Y')
        session.until('DISK PACK IS COMPLETELY FORMATTED')
        session.finish()
        passed('9374 removable pack formatted with zero controller errors')
    with machine(output) as sim:
        sim.load(tapes[2])
        session = Session(sim, output, 'generation')
        session.answer('ON WHICH -LOGICAL- DRIVE', '0')
        session.answer('COMPLETE DOS GENERATION', 'Y')
        session.answer('STARTS BY ERASING', 'Y')
        session.answer('DO YOU MIND LOSING', 'N')
        session.answer('LOCK OUT ANY CYLINDERS', 'N')
        session.until('CASSETTE DOS GENERATION COMPLETE')
        session.until('READY', after='CASSETTE DOS GENERATION COMPLETE')
        session.finish()
        passed('Full DOS.D generation completed and reached READY')
        for n in (3, 4, 5):
            sim.command('tape-stop')
            sim.command(f'tape 1 {tapes[n]}')
            session = Session(sim, output, f'utility-{n}')
            session.keys('MIN;AO:D0\n')
            last_file = {3: 'FILE MIN/CMD', 4: 'FILE UTILITY/SYS', 5: 'FILE DOSD/RFM'}[n]
            session.until('MULTIPLE IN COMPLETED', after=last_file)
            session.until('READY', after='MULTIPLE IN COMPLETED')
            session.finish()
            passed(f'Tape {n}: MIN completed and returned to READY')
        sim.command('tape-stop')
        sim.command(f'tape-create 1 {output / "boot.tap"}')
        session = Session(sim, output, 'uboot')
        session.keys('UBOOT\n')
        session.until('TAP ENTER KEY')
        session.keys('\n')
        session.until('BOOT VERIFIED - MAKE ANOTHER?')
        session.keys('N\n')
        session.until('READY', after='BOOT VERIFIED')
        session.finish()
        passed('UBOOT wrote and verified a boot cassette')
    passed(initialize_fixed(output))
    passed(boot(output, all_drives=True))
    return checks


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', default=str(ROOT / 'dp2200sim/dos-d-results' /
                                               datetime.now().strftime('%Y%m%d-%H%M%S')))
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--boot-only', action='store_true',
                        help='boot and catalogue an existing installation selected by --output')
    action.add_argument('--initialize-fixed', action='store_true',
                        help='DOSGEN drive 1 on copies, verify, and replace fixed.dsk with a backup')
    args = parser.parse_args()
    if args.initialize_fixed:
        print(repair_fixed(args.output), flush=True)
    elif args.boot_only:
        print(boot(args.output, all_drives=True), flush=True)
    else:
        install(args.output)
