"""Independently decode RIMTEST's address-check area from the original tape.

This small decoder uses the documented instruction encodings, not the simulator
disassembler or its execution trace. It only supports instructions in this area.
"""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TAPE = ROOT / 'tapes/diagnostics/RIMTEST.tap'
ANCHOR = 0o26357
SIGNATURE = bytes.fromhex('0e 01 c2 a9 51 28 41 a3 c2 68 02 2d a1 48 0e 2d '
                          '44 06 2d a1 68 0e 2d 49 02 40 f1 2c 49 b1 07 a8 07')
REGISTERS = 'ABCDEHLM'
MATH = ('AD', 'AC', 'SU', 'SB', 'ND', 'XR', 'OR', 'CP')


def evaluate_check(chosen, responds):
    """Evaluate the raw routine using only its documented ISA operations.

    responds(bus_value) supplies DA; this does not call simulator CPU/I/O code.
    Returns success and the actual probe sequence executed by the routine.
    """
    a, b, c, d = 0, 0, chosen, 0o40
    zero = carry = False
    pc, probes = ANCHOR, []
    for _ in range(200):
        index = pc - ANCHOR
        op = SIGNATURE[index]
        if op == 0o016:
            b = SIGNATURE[index + 1]
            pc += 2
        elif op == 0o302:
            a = c  # Load preserves all flags.
            pc += 1
        elif op == 0o251:
            a ^= b
            zero, carry = a == 0, False
            pc += 1
        elif op == 0o121:
            bus = a
            pc += 1
        elif op == 0o050:
            pc += 1
        elif op == 0o101:
            a = 0o40 if responds(bus) else 0
            probes.append((bus, bool(a)))
            pc += 1
        elif op in (0o243, 0o241):
            a &= d if op == 0o243 else b
            zero, carry = a == 0, False
            pc += 1
        elif op in (0o150, 0o110, 0o100, 0o104):
            target = SIGNATURE[index + 1] | SIGNATURE[index + 2] << 8
            take = {0o150: zero, 0o110: not zero,
                    0o100: not carry, 0o104: True}[op]
            pc = target if take else pc + 3
        elif op == 0o111:
            if SIGNATURE[index + 1] == 0o002:
                carry = bool(b & 128)
                b = ((b << 1) | int(carry)) & 255
            elif SIGNATURE[index + 1] == 0o261:
                b |= b
                zero, carry = b == 0, False
            else:
                raise ValueError('Unknown extended opcode')
            pc += 2
        elif op == 0o250:
            a, zero, carry = 0, True, False
            pc += 1
        elif op == 0o007:
            return not zero, probes
        else:
            raise ValueError(f'Unknown routine opcode {op:03o}')
    raise ValueError('Routine did not return within its bounded instruction count')


def decode(data):
    op = data[0]
    if op == 0o111:
        if data[1] == 0o002:
            return 'SLCB', 2
        if data[1] == 0o261:
            return 'ORBB', 2
        raise ValueError('Unknown B-prefix instruction')
    single = {0o007: 'RET', 0o050: 'EI', 0o101: 'INPUT',
              0o121: 'EX ADR', 0o151: 'EX BEEP'}
    if op in single:
        return single[op], 1
    if op < 0o100 and op & 7 == 6:
        return f'L{REGISTERS[(op >> 3) & 7]} {data[1]:03o}', 2
    if op < 0o100 and op & 7 == 4:
        return f'{MATH[(op >> 3) & 7]} {data[1]:03o}', 2
    branches = {0o100: 'JFC', 0o110: 'JFZ', 0o150: 'JTZ',
                0o104: 'JMP', 0o106: 'CALL'}
    if op in branches:
        target = data[1] | data[2] << 8
        return f'{branches[op]} {target:06o}', 3
    if op >= 0o300:
        return f'L{REGISTERS[(op >> 3) & 7]}{REGISTERS[op & 7]}', 1
    if 0o200 <= op < 0o300:
        return f'{MATH[(op >> 3) & 7]}{REGISTERS[op & 7]}', 1
    raise ValueError(f'Unknown instruction {op:03o}')


def main():
    tape = TAPE.read_bytes()
    offset = tape.find(SIGNATURE)
    if offset < 0 or tape.find(SIGNATURE, offset + 1) >= 0:
        raise ValueError('Expected a unique original address-check byte sequence')
    print('RIMTEST address setup/check: independent decode of original tape bytes')
    print('SHA-256: ' + hashlib.sha256(tape).hexdigest())
    print(f'Routine anchor: PC {ANCHOR:06o}, tape file offset {offset} decimal')
    print('Instruction addresses, bytes and operands below are octal.\n')
    address = 0o26203
    while address < 0o26420:
        position = offset + address - ANCHOR
        mnemonic, size = decode(tape[position:position + 3])
        raw = ' '.join(f'{value:03o}' for value in tape[position:position + size])
        print(f'{address:06o}  {raw:<11} {mnemonic}')
        address += size


if __name__ == '__main__':
    main()
