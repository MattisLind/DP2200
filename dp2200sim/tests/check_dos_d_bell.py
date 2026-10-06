"""Reproduce DOS.D's missing-pack bell and check booting with both 9374 packs."""
import argparse
from pathlib import Path
import shutil
import tempfile

from harness import Harness
from install_dos_d import Session


def check(installation):
    installation = Path(installation).resolve()
    for name in ('removable.dsk', 'fixed.dsk', 'boot.tap'):
        if not (installation / name).is_file():
            raise FileNotFoundError(installation / name)
    # DOS may update a writable pack. Always use isolated copies.
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        for pack in ('absent', 'blank', 'initialized'):
            fixed = pack != 'absent'
            for name in ('removable.dsk', 'fixed.dsk', 'boot.tap'):
                shutil.copyfile(installation / name, work / name)
            if pack == 'blank':
                with (work / 'fixed.dsk').open('wb') as blank:
                    blank.truncate((installation / 'fixed.dsk').stat().st_size)
            with Harness() as sim:
                sim.command('cpu 5500')
                sim.command(f'pc {0o170036}')
                assert sim.command('run 10000000', timeout=60)['halted']
                sim.command('disk-model 9374')
                sim.command(f'disk 0 0 {work / "removable.dsk"}')
                if fixed:
                    sim.command(f'disk 1 1 {work / "fixed.dsk"}')
                sim.load(work / 'boot.tap')
                session = Session(sim, work, f'fixed-{pack}')
                session.until('READY')
                session.run()
                assert session.state['beeps'] == 0, 'Bell before any command'
                # A catalogue alone did not expose the missing-pack problem.
                session.keys('CAT :D0\n')
                session.until('READY', after='SYSTEM0/SYS')
                session.run()
                assert session.state['beeps'] == 0, 'Bell after initial CAT'
                # HELP is absent on this installation and should return WHAT?.
                session.keys('HELP\n')
                session.until('WHAT?')
                session.run()
                first = session.state['beeps']
                session.run()
                second = session.state['beeps']
                if fixed:
                    assert first == second == 0, 'Repeating bell with both packs'
                    session.keys('CAT :D0\n')
                    session.until('READY', after='SYSTEM0/SYS')
                    session.run()
                    assert session.state['beeps'] == 0, 'Bell after subsequent CAT'
                    assert 'FAILURE IN SYSTEM DATA' not in session.state['screen']
                    # Plain CAT also scans the online but uninitialized fixed pack.
                    session.keys('CAT\n')
                    if pack == 'blank':
                        session.until('FAILURE IN SYSTEM DATA AT 021444')
                        session.until('READY', after='DRIVE 01.')
                        assert 'DRIVE 01.' in session.state['screen']
                    else:
                        session.until('READY', after='DRIVE  1')
                        session.run()
                        assert 'FAILURE IN SYSTEM DATA' not in session.state['screen']
                        assert session.state['beeps'] == 0
                    session.keys('CAT :D0\n')
                    session.until('READY', after='SYSTEM0/SYS')
                    session.run()
                    assert 'FAILURE IN SYSTEM DATA' not in session.state['screen']
                    assert session.state['beeps'] == 0
                    print('PASS: both packs attached; CAT, unknown HELP, and CAT stay quiet')
                    print(f'PASS: plain CAT with {pack} fixed pack; CAT :D0 succeeds')
                else:
                    assert second > first > 0, 'Missing-pack bell no longer reproduced'
                    print(f'PASS: absent fixed pack reproduces continuing bell ({first} -> {second})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', default=str(Path(__file__).resolve().parents[1] /
                                               'dos-d-results/dos-d-2.6'),
                        help='existing DOS.D installation directory')
    check(parser.parse_args().output)
