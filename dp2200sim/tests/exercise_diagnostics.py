"""Selected inventory experiments; observations are not blanket diagnostic passes."""
import argparse
import json
from pathlib import Path
import tempfile
from harness import Harness

ROOT = Path(__file__).resolve().parents[2]


def exercise(name, model, actions, setup=()):
    result = {'tape': name + '.tap', 'cpu': model, 'actions': []}
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        with Harness() as sim:
            def command(text):
                text = text.replace('{work}', str(work)).replace('{root}', str(ROOT))
                state = sim.command(text, timeout=45)
                result['actions'].append({'command': text.replace(str(work), '{work}').replace(str(ROOT), '{root}'),
                    **{key: state[key] for key in ('pc', 'halted', 'instructions', 'time_ns', 'keyboard_ready', 'beeps')},
                    'screen': '\n'.join(line.rstrip() for line in state['screen'].splitlines()).rstrip(),
                    **({'disk': state['disk']} if 'disk' in state else {}),
                    **({'debug': state['debug']} if state['debug'] else {})})
                return state
            try:
                command(f'cpu {model}')
                if model != 2200:
                    command(f'pc {0o170036}')
                    command('run 1000000')
                for text in setup:
                    command(text)
                command(f'load {{root}}/tapes/diagnostics/{name}.tap')
                command('run 5000000')
                for action, value in actions:
                    if action == 'keys':
                        for character in value:
                            sim.key(character)
                            result['actions'].append({'host_key': character})
                            command('run 100000')
                    else:
                        command(value)
            except Exception as exc:
                result['error'] = str(exc)
    return result


def scenarios():
    for tape, model, count in [('DIAG2200_V1.1', 2200, 5), ('DIAG5500_V1.1', 5500, 6), ('DIAG6600_V1.1', 6600, 6)]:
        for selection in range(1, count + 1):
            yield tape, model, [('keys', str(selection) + '\n'), ('command', 'run 25000000'), ('command', 'inspect')], ()
    for tape, model, selections in [('DIAG5500_V1.1', 5500, [6]), ('DIAG6600_V1.1', 6600, range(2, 7))]:
        for selection in selections:
            yield tape, model, [('keys', str(selection) + '\n'), ('command', 'run 25000000'),
                ('command', 'button keyboard 1'), ('command', 'run 1000000'),
                ('command', 'button keyboard 0'), ('command', 'run 25000000'), ('command', 'inspect')], ()
    for tape in ('5500PROCTEST_V2.1', 'TSTUBE55_V1.2'):
        yield tape, 5500, [('command', 'inspect'), ('command', 'button keyboard 1'),
            ('command', 'run 1000000'), ('command', 'button keyboard 0'), ('command', 'run 25000000')], ()
    for tape in ('COPY06_1', 'EM3780TRACE_V3.1', 'LGO_A'):
        yield tape, 5500, [('command', 'inspect'), ('command', 'continue'), ('command', 'run 5000000')], ()
    yield 'PCMTEST_V1.1', 2200, [('command', 'run 100000000')], ()
    yield 'HRLYMEMT', 2200, [('command', 'run 100000000')], ()
    yield 'EXTDISPTEST', 2200, [('keys', 'Z'), ('command', 'run 1000000')], ()
    yield 'TSTDIS', 2200, [('command', text) for _ in range(3) for text in (
        'button display 1', 'run 100000', 'button display 0', 'run 100000')], ()
    yield 'TSTKEY', 2200, [('keys', 'N\n'), ('command', 'run 1000000')], ()
    yield 'EXRCASS_V1.1', 2200, [('keys', '2S1S'), ('command', 'run 1000000')], ()
    yield 'MPXTEST', 2200, [('keys', '105\n'), ('command', 'run 1000000')], ()
    yield 'TST404', 2200, [('keys', 'N\nA\n'), ('command', 'run 1000000')], ()
    yield 'MPXTEST', 2200, [('keys', '151\n'), ('command', 'run 1000000')], ()
    yield 'TST404', 2200, [('keys', 'N\n'), ('command', 'run 1000000')], ()
    yield 'EXRIBM_V2.3.A', 2200, [('keys', '9Y'), ('command', 'run 1000000')], ()
    for tape in ('DOSC_BOOT', 'UBOOT-2', 'UBOOT'):
        yield tape, 2200, [('command', 'run 25000000')], ('floppy 0 {root}/tapes/DOS.C/003.IMD',)
    yield 'SURVAR_V1.1', 5500, [('keys', '0DR\n'), ('command', 'run 100000000'), ('command', 'disk-state')], (
        'disk-model 9370', 'disk 0 0 {work}/disk.dsk', 'disk-protect 0 1', 'printer {work}/printer.txt')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    results = []
    for name, model, actions, setup in scenarios():
        result = exercise(name, model, actions, setup)
        results.append(result)
        last = next((a for a in reversed(result['actions']) if 'screen' in a), {})
        print(name, model, result.get('error') or repr(last.get('screen', '')[:140]), flush=True)
    args.output.write_text(json.dumps({'experiments': results}, indent=2) + '\n')


if __name__ == '__main__':
    main()
