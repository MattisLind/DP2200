"""Trace original RIMTEST address checks against the current mask decoder.

Run from any directory after building dp2200sim-headless. Temporary extra
fixtures include two and six nominally addressed modules and right-input
experiments with only the left module. Main-screen entry is not a full pass.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]
TAPE = ROOT / 'tapes/diagnostics/RIMTEST.tap'
LEFT, RIGHT = 0o234, 0o232


def send(sim, text):
    for character in text:
        sim.key(character)
        sim.command('run 100000')


def snapshot(state):
    return {key: state[key] for key in ('pc', 'halted', 'instructions', 'screen')}


def probe(addresses=(LEFT, RIGHT)):
    with tempfile.TemporaryDirectory() as directory, Harness() as sim:
        sim.command('cpu 5500')
        devices = list(addresses)
        for node, address in enumerate(devices, 1):
            sim.command(f'rim {address} {node}')
        sim.command('pc 61470')  # Initialize the real restart ROM.
        sim.command('run 10000000')
        sim.load(TAPE)
        sim.command('run 5000000')
        send(sim, '1\r2\r234')
        result = {'devices_octal': [f'{address:03o}' for address in devices]}
        for side in ('left', 'right'):
            trace = Path(directory) / f'{side}.trace'
            sim.command(f'trace {trace}')
            sim.key('\r')
            sim.command('run 2000')
            # Switching traces flushes/closes the evidence before reading it.
            sim.command('trace /dev/null')
            state = sim.command('run 100000')
            lines = trace.read_text().splitlines()
            result[side] = snapshot(state)
            result[side]['address_trace'] = [line.rstrip() for line in lines
                if line[:6].isdigit() and 0o26200 <= int(line[:6], 8) <= 0o26417]
            result[side]['probe_addresses_octal'] = [
                line.split('A=')[1].split()[0] for line in lines
                if line.startswith('026363 ') and 'EX_ADR' in line]
            if side == 'left':
                result['routine_bytes_octal'] = [f'{value:03o}' for value in
                    sim.command(f'memory {0o26357} 33')['memory']]
                if 'I/O Address for RIM (Right)' not in state['screen']:
                    break
                send(sim, '232')
        return result


def single_rim(right_id='2\r', right_address=None):
    """Right-input experiments with exactly one real module attached."""
    with tempfile.TemporaryDirectory() as directory, Harness() as sim:
        sim.command('cpu 5500')
        devices = [LEFT]
        for node, address in enumerate(devices, 1):
            sim.command(f'rim {address} {node}')
        sim.command('pc 61470')
        sim.command('run 10000000')
        sim.load(TAPE)
        sim.command('run 5000000')
        send(sim, '1\r')
        send(sim, right_id)
        result = {'devices_octal': [f'{address:03o}' for address in devices],
                  'right_id_input': repr(right_id),
                  'after_right_id': snapshot(sim.command('run 1000'))}
        if 'I/O Address for RIM (Left)' not in result['after_right_id']['screen']:
            return result
        send(sim, '234')
        trace = Path(directory) / 'left.trace'
        sim.command(f'trace {trace}')
        sim.key('\r')
        sim.command('run 2000')
        sim.command('trace /dev/null')
        result['after_left_address'] = snapshot(sim.command('run 100000'))
        result['left_address_trace'] = [line.rstrip() for line in trace.read_text().splitlines()
            if line[:6].isdigit() and 0o26200 <= int(line[:6], 8) <= 0o26417]
        if right_address is not None:
            if 'I/O Address for RIM (Right)' not in result['after_left_address']['screen']:
                raise AssertionError('The left module did not complete its address check')
            trace = Path(directory) / 'right.trace'
            sim.command(f'trace {trace}')
            send(sim, right_address)
            sim.command('trace /dev/null')
            result['right_address_input'] = repr(right_address)
            result['after_right_address'] = snapshot(sim.command('run 1000'))
            result['right_address_trace'] = [line.rstrip() for line in trace.read_text().splitlines()
                if line[:6].isdigit() and 0o26270 <= int(line[:6], 8) <= 0o26417]
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = {
        'tape': str(TAPE.relative_to(ROOT)),
        'sha256': hashlib.sha256(TAPE.read_bytes()).hexdigest(),
        'method': 'Unmodified tape, CPU 5500, IDs 1/2, addresses 234/232 octal. '
                  'Each configured mask has four set bits; all required bits '
                  'must be present on the bus. No guest code is patched.',
        'two_rims': probe(),
        'six_rims': probe((LEFT, RIGHT, 0o231, 0o254, 0o252, 0o251)),
        'decoder': '(bus & configured_mask) == configured_mask',
        'one_rim': single_rim(),
        'right_id_blank': single_rim(right_id='\r'),
        'right_id_zero': single_rim(right_id='0\r'),
        'right_id_star': single_rim(right_id='*\r'),
        'left_only_right_blank': single_rim(right_address='\r'),
        'left_only_right_zero': single_rim(right_address='0\r'),
        'left_only_right_star': single_rim(right_address='*\r'),
        'left_only_right_absent': single_rim(right_address='232\r'),
        'left_only_right_alias': single_rim(right_address='235\r'),
    }
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    for name in ('two_rims', 'six_rims'):
        print(name + ': ' + ', '.join(
            f"{side} probes={','.join(result[name][side]['probe_addresses_octal'])}"
            for side in ('left', 'right') if side in result[name]))


if __name__ == '__main__':
    main()
