"""Read-only tape audit and bounded boot probes; not a diagnostic pass/fail suite."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]


def inspect_tape(path):
    data = path.read_bytes()
    offset = 0
    records = []
    issues = []
    headers = []
    numeric = 0
    bad_checksums = []
    bad_addresses = []
    while offset < len(data):
        start = offset
        if offset + 4 > len(data):
            issues.append(f"truncated length at {offset}")
            break
        size = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        if offset + size + 4 > len(data):
            issues.append(f"truncated record at {start}, length {size}")
            break
        payload = data[offset:offset + size]
        offset += size
        trailer = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        if trailer != size:
            issues.append(f"length mismatch at {start}: {size}/{trailer}")
        records.append(payload)
        if payload[:2] == b'\x81\x7e':
            if len(payload) != 4 or payload[2] != (payload[3] ^ 255):
                issues.append(f"bad file header at {start}")
            else:
                headers.append({'offset': start, 'file': payload[2]})
        elif payload[:2] == b'\xc3\x3c':
            numeric += 1
            if len(payload) < 8:
                issues.append(f"short numeric record at {start}")
                continue
            xor, circulated = payload[2:4]
            for byte in payload[4:]:
                xor ^= byte
                circulated ^= byte
                circulated = (circulated >> 1) | ((circulated & 1) << 7)
            if xor or circulated:
                bad_checksums.append(start)
            if payload[4] != payload[6] ^ 255 or payload[5] != payload[7] ^ 255:
                bad_addresses.append(start)
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'records': len(records), 'bootstrap_bytes': len(records[0]) if records else 0,
            'headers': headers, 'numeric_records': numeric, 'framing_issues': issues,
            'bad_numeric_checksums': bad_checksums, 'bad_numeric_addresses': bad_addresses}


def probe(path, model, budget):
    result = {'cpu': model, 'budget': budget, 'snapshots': []}
    try:
        with Harness() as sim:
            sim.command(f'cpu {model}')
            if model != 2200:
                sim.command(f'pc {0o170036}')
                state = sim.command('run 1000000', timeout=30)
                result['rom_halted'] = state['halted']
            sim.load(path)
            baseline = sim.command('state')['instructions']
            elapsed = 0
            for target in sorted(set([min(100000, budget), min(1000000, budget), budget])):
                state = sim.command(f'run {target - elapsed}', timeout=30)
                elapsed = target
                result['snapshots'].append({key: state[key] for key in
                    ('pc', 'halted', 'instructions', 'time_ns', 'keyboard_ready', 'beeps')} |
                    {'screen': '\n'.join(line.rstrip() for line in state['screen'].splitlines()).rstrip(),
                     'tape_instructions': state['instructions'] - baseline})
                if state['halted']:
                    break
    except Exception as exc:
        result['error'] = str(exc)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--budget', type=int, default=5000000)
    args = parser.parse_args()
    if not 1 <= args.budget <= 100000000:
        parser.error('budget must be 1..100000000')
    inventory = []
    for path in sorted((ROOT / 'tapes/diagnostics').glob('*.tap')):
        entry = {'tape': path.name, 'structure': inspect_tape(path),
                 'probes': [probe(path, model, args.budget) for model in (2200, 5500, 6600)]}
        inventory.append(entry)
        print(path.name, ', '.join(f"{p['cpu']}: " + (p.get('error') or
            ('halt' if p['snapshots'][-1]['halted'] else 'running')) for p in entry['probes']), flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'method': 'read-only; no external media, no input; ROM init on 5500/6600',
                                      'budget': args.budget, 'tapes': inventory}, indent=2) + '\n')


if __name__ == '__main__':
    main()
