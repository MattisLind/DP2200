"""Verify EXRCASS transport and report TAPTIM measurements on disposable media."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def screen(state):
    return '\n'.join(line.rstrip() for line in state['screen'].splitlines()).rstrip()


def run(sim, count=100000):
    state = sim.command(f'run {count}', timeout=30)
    require(not state['halted'], f"Unexpected diagnostic HALT: {screen(state)}")
    return state


def key(sim, character, count=100000):
    sim.key(character)
    state = run(sim, count)
    require(not state['keyboard_ready'], f'Key {character!r} was not consumed')
    return state


def cassette(sim):
    return sim.command('cassette-state')['cassette']


def framed(payload):
    length = struct.pack('<I', len(payload))
    return length + payload + length


def records(data):
    result = []
    offset = 0
    while offset < len(data):
        require(offset + 4 <= len(data), 'Truncated tape length')
        size = struct.unpack_from('<I', data, offset)[0]
        require(0 < size <= 65536 and offset + size + 8 <= len(data), 'Invalid tape record')
        require(struct.unpack_from('<I', data, offset + size + 4)[0] == size, 'Tape trailer mismatch')
        result.append(data[offset + 4:offset + size + 4])
        offset += size + 8
    return result


def run_exrcass():
    image = ROOT / 'tapes/diagnostics/EXRCASS_V1.1.tap'
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    observations = []
    with tempfile.TemporaryDirectory() as work, Harness() as sim:
        sim.load(image)
        require('EXRCASS 1.1' in run(sim, 5000000)['screen'], 'EXRCASS menu missing')
        sim.command('tape-stop')
        paths = []
        # Include one-byte records and mixed bits; use a long image so STOP can
        # interrupt transport before an endpoint, rather than trivially at EOF.
        for deck in (0, 1):
            path = Path(work) / f'{deck}.tap'
            payload = b''.join(framed(part) for part in
                [bytes([0x96 + deck]), bytes(range(256)), bytes([0x35 + deck])]) * 8
            path.write_bytes(payload)
            sim.command(f'tape {deck} {path}')
            paths.append((path, payload))
        for deck in (0, 1):
            key(sim, str(deck + 1))
            start = cassette(sim)
            require(start['deck'] == deck, 'Wrong selected EXRCASS deck')
            other = start['decks'][1 - deck]['position']
            key(sim, 'F', 1000)
            run(sim, 30000)
            moving = cassette(sim)
            require(moving['running'], 'EXRCASS forward transport did not start')
            require(moving['decks'][deck]['position'] > 0, 'EXRCASS did not read forward')
            key(sim, 'S', 1000)
            stopped = cassette(sim)
            require(not stopped['running'], 'EXRCASS STOP left callbacks running')
            position = stopped['decks'][deck]['position']
            run(sim, 100000)
            require(cassette(sim)['decks'][deck]['position'] == position, 'Stopped tape moved')
            key(sim, 'F', 1000)
            run(sim, 5000000)
            end = cassette(sim)
            require(not end['running'] and end['status'] & 3 == 3, 'Forward EOT/ready missing')
            require(end['decks'][deck]['position'] == len(paths[deck][1]), 'Wrong EOF position')
            key(sim, 'B', 1000)
            run(sim, 5000000)
            beginning = cassette(sim)
            require(not beginning['running'] and beginning['status'] & 3 == 3, 'Reverse BOT/ready missing')
            require(beginning['decks'][deck]['position'] == 0, 'Reverse did not reach BOT')
            key(sim, 'F', 1000)
            run(sim, 10000)
            key(sim, 'S', 1000)
            key(sim, 'R', 1000)
            run(sim, 10000)
            rewind = cassette(sim)
            require(not rewind['running'] and rewind['decks'][deck]['position'] == 0,
                    'EXRCASS rewind did not finish at BOT')
            require(rewind['decks'][1 - deck]['position'] == other, 'Other deck moved')
            require(paths[deck][0].read_bytes() == paths[deck][1], 'Read-only tape changed')
            observations.append({'deck': deck, 'bytes': len(paths[deck][1]), 'forward': end,
                                 'reverse': beginning, 'rewind': rewind, 'stop_position': position})
    require(hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'Diagnostic image changed')
    return {'tape': image.name, 'sha256': digest, 'functional_pass': True, 'decks': observations}


def run_taptim(deck=0, mode='T'):
    image = ROOT / 'tapes/diagnostics/TAPTIM_V1.5.tap'
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory() as work, Harness() as sim:
        sim.load(image)
        require('TAPTIM 1.5' in run(sim, 5000000)['screen'], 'TAPTIM menu missing')
        sim.command('tape-stop')
        paths = [Path(work) / f'{i}.tap' for i in (0, 1)]
        for i, path in enumerate(paths):
            sim.command(f'tape-create {i} {path}')
        state = key(sim, '\n')
        require('NEXT TEST?' in state['screen'], 'TAPTIM initialization did not finish')
        trace = Path(work) / 'io.log'
        if mode == 'E':
            sim.command(f'io-trace {trace}')
        for character in f'{mode}{deck + 1}\n':
            state = key(sim, character)
        for _ in range(25 if mode == 'E' else 10):
            state = run(sim, 1000000)
            require('FAILURE' not in state['screen'] and 'RECEIVED:' not in state['screen'],
                    f'TAPTIM reported data error: {screen(state)}')
            if mode in ('I', 'S') or (mode == 'T' and re.search(r'PASS:\s+00[2-9]', state['screen'])):
                break
        text = screen(state)
        measured = []
        if mode == 'T':
            for line in text.splitlines():
                match = re.search(r'\|\s*(.*?)\s+(\d{3})\s+(\d{3})\s+(\d{3})(?:\s+(\d+)\*)?\s*$', line)
                if match:
                    label, nominal, tolerance, value, errors = match.groups()
                    nominal, tolerance, value = map(int, (nominal, tolerance, value))
                    measured.append({'test': label, 'nominal': nominal, 'tolerance': tolerance,
                                     'measured': value, 'error_count': int(errors or 0),
                                     'within_tolerance': abs(value - nominal) <= tolerance})
            require(len(measured) == 7, 'Incomplete TAPTIM timing table')
            written = records(paths[deck].read_bytes())
            require(any(len(record) == 600 and record == b'\xff' * 600 for record in written)
                    and any(len(record) == 100 and record == b'\xff' * 100 for record in written),
                    'TAPTIM did not produce its 600-/100-byte test patterns')
        else:
            written = records(paths[deck].read_bytes())
        read_bytes = []
        if mode == 'E':
            read_bytes = re.findall(rf'READ deck={deck} data=([0-7]{{3}})', trace.read_text())
            require(len(read_bytes) >= 1000, 'Endurance did not read enough pattern bytes')
            require(set(read_bytes) == {'377', '000'}, 'Endurance did not read its alternating pattern')
            require(len(written) >= 20 and all(record == (b'\xff\x00' * (len(record) // 2)
                    + (b'\xff' if len(record) % 2 else b'')) for record in written),
                    'Endurance recorded wrong pattern')
        observation = cassette(sim)
        output = {'tape': image.name, 'sha256': digest, 'deck': deck, 'mode': mode,
                  'screen': text, 'cassette': observation, 'measurements': measured,
                  'record_lengths': [len(record) for record in written],
                  'timing_pass': all(row['within_tolerance'] and not row['error_count']
                                     for row in measured) if measured else None,
                  'endurance_pass': None, 'bounded_data_pass': True if mode == 'E' else None,
                  'pattern_bytes_read': len(read_bytes), 'time_ns': state['time_ns']}
    require(hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'Diagnostic image changed')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dp2200sim/cassette-results.json')
    parser.add_argument('--require-timing', action='store_true', help='exit unsuccessfully if TAPTIM timing fails')
    args = parser.parse_args()
    result = {'exrcass': run_exrcass(), 'taptim': [run_taptim(deck) for deck in (0, 1)],
              'endurance': [run_taptim(deck, 'E') for deck in (0, 1)],
              'limitations': [run_taptim(0, mode) for mode in ('I', 'S')]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('EXRCASS: both decks passed forward/reverse, endpoints, STOP, rewind and media integrity checks')
    for item in result['taptim']:
        print(f"TAPTIM T{item['deck'] + 1}: pattern record verified; timing " +
              ('PASS' if item['timing_pass'] else 'FAIL'))
    for item in result['endurance']:
        print(f"TAPTIM E{item['deck'] + 1}: bounded data check passed; {item['pattern_bytes_read']} bytes read")
    print('Full endurance/IRG/full-length speed not certified; see', args.output)
    if args.require_timing and not all(item['timing_pass'] for item in result['taptim']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
