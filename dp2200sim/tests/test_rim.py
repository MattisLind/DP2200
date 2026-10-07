"""9483 Stage 1: real 5500 instructions against isolated register/buffer devices."""
from pathlib import Path
import re
import struct
import tempfile
import unittest

from harness import Harness

RIM = 0o234
SECOND = 0o232
RESERVED = (RIM, SECOND, 0o231, 0o254, 0o252, 0o251)
TA, TMA, RECON, TPE, POR, DA, IPE, RI = (1 << bit for bit in range(8))


def la(value):
    return [0o006, value]


def command(opcode, value):
    return la(value) + [opcode]


def hl(address):
    return [0o056, address >> 8, 0o066, address & 255]


def page(number):
    return command(0o131, (number << 3) | 3)


def pointer(offset):
    return command(0o137, offset)


def registers(sim):
    debug = sim.command('inspect')['debug']
    match = re.search(r'bank=0 ((?:\d+ ){8})C=(\d) Z=(\d) S=(\d) P=(\d)', debug)
    if not match:
        raise AssertionError(debug)
    return list(map(int, match[1].split())), tuple(map(int, match.groups()[1:]))


class RimTests(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)

    def simulator(self):
        sim = Harness()
        self.addCleanup(sim.__exit__)
        sim.command('cpu 5500')
        sim.command(f'rim {RIM} 1')
        return sim

    def load(self, sim, code, data=b''):
        payload = bytes(code) + b'\0'
        if data:
            payload = payload.ljust(4096, b'\0') + data
        size = struct.pack('<I', len(payload))
        tape = Path(self.work.name) / 'program.tap'
        tape.write_bytes(size + payload + size)
        sim.load(tape)

    def execute(self, sim, code, data=b''):
        self.load(sim, code, data)
        state = sim.command('run 100000')
        self.assertTrue(state['halted'], state)
        self.assertEqual(state['pc'], len(code) + 1, 'Premature instruction halt')
        return state

    def test_attachment_validation_and_cpu_scope(self):
        with Harness() as sim:
            with self.assertRaises(RuntimeError):
                sim.command(f'rim {RIM} 1')
        sim = self.simulator()
        for address, node in ((RIM, 2), (RIM ^ 1, 2), (SECOND, 1),
                              (0xe1, 2), (0xe0, 2), (0, 2), (255, 2), (-1, 2),
                              (256, 2), (SECOND, 0), (SECOND, 256)):
            with self.subTest(address=address, node=node), self.assertRaises(RuntimeError):
                sim.command(f'rim {address} {node}')
        sim.command(f'rim {SECOND} 2')
        self.assertEqual(sim.command(f'rim-state {SECOND}')['rim']['node'], 2)
        with Harness() as other:
            other.command('cpu 6600')
            with self.assertRaises(RuntimeError):
                other.command(f'rim {RIM} 1')

    def test_all_pages_wrap_and_status_reads_preserve_pointer(self):
        sim = self.simulator()
        code = command(0o121, RIM)
        for number in range(4):
            code += page(number) + pointer(254)
            for value in (0x80 + number, 0x40 + number, 0x20 + number):
                code += command(0o127, value)
        # Status mode WRITE still accesses the buffer. STATUS INPUT must not
        # advance its byte pointer; DATA and PIN reads must each advance it.
        code += page(0) + pointer(10) + [0o123] + command(0o127, 0xab)
        code += [0o101, 0o101] + page(0) + pointer(10) + [0o103]
        code += hl(8192) + [0o370]
        for number in range(4):
            code += page(number) + pointer(254) + [0o123, 0o101, 0o125]
            for index in range(3):
                code += [0o101] + hl(8193 + number * 3 + index) + [0o370]
        self.execute(sim, code)
        expected = [0xab] + [value + number for number in range(4) for value in (0x80, 0x40, 0x20)]
        self.assertEqual(sim.command('memory 8192 13')['memory'], expected)
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['pointer'], 1)

    def test_two_devices_keep_independent_buffers_and_selection(self):
        sim = self.simulator()
        sim.command(f'rim {SECOND} 2')
        code = []
        for address, value in ((RIM, 0x55), (SECOND, 0xaa)):
            code += command(0o121, address) + page(2) + pointer(7) + command(0o127, value)
        for index, address in enumerate((RIM, SECOND)):
            code += command(0o121, address) + pointer(7) + [0o101]
            code += hl(8192 + index) + [0o370]
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 2')['memory'], [0x55, 0xaa])
        self.assertEqual(sim.command(f'rim-state {SECOND}')['rim']['pointer'], 8)

    def test_address_select_restores_status_mode_without_moving_pointer(self):
        sim = self.simulator()
        code = command(0o121, RIM) + pointer(7) + command(0o127, 0xab)
        code += pointer(7) + command(0o121, RIM) + [0o101] + hl(8192) + [0o370]
        code += [0o125, 0o101] + hl(8193) + [0o370]
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 2')['memory'], [TA | POR | DA | RI, 0xab])
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['pointer'], 8)

    def test_bit_zero_alias_shares_buffer_pointer_pages_and_status(self):
        sim = self.simulator()
        code = command(0o121, RIM) + page(2) + pointer(7)
        code += command(0o127, 0x55)
        # WRITE via the alias must continue at the shared pointer, on page 2.
        code += command(0o121, RIM ^ 1) + command(0o127, 0xaa)
        code += command(0o131, 6) + [0o101] + hl(8192) + [0o370]
        code += pointer(7) + [0o101] + hl(8193) + [0o370]
        # Nominal EX ADR restores STATUS; shared pointer is now 8.
        code += command(0o121, RIM) + [0o101] + hl(8194) + [0o370]
        code += [0o125, 0o101] + hl(8195) + [0o370]
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 4')['memory'],
                         [TA | DA | RI, 0x55, TA | DA | RI, 0xaa])
        state = sim.command(f'rim-state {RIM}')['rim']
        self.assertEqual(state, sim.command(f'rim-state {RIM ^ 1}')['rim'])
        self.assertEqual((state['processor_page'], state['pointer']), (2, 9))

    def test_high_address_alias_does_not_select_before_ex_adr(self):
        sim = self.simulator()
        sim.command('rim 198 2')  # Four ones; bus 255 selects both attached RIMs.
        self.execute(sim, [0o101] + hl(8192) + [0o370])
        self.assertEqual(sim.command('memory 8192 1')['memory'], [0])
        self.assertEqual(sim.command('rim-state 198')['rim']['pointer'], 0)
        with self.assertRaises(RuntimeError):
            sim.command('rim-state 255')  # Ambiguous host inspection.

    def test_all_bus_values_for_each_reserved_mask(self):
        for mask in RESERVED:
            with self.subTest(mask=oct(mask)), Harness() as sim:
                sim.command('cpu 5500')
                sim.command(f'rim {mask} 1')
                code = []
                for bus in range(256):
                    code += command(0o121, bus)
                    if bus in (0xf0, 0xe1, 0x3c, 0x96, 0x5a, 0xc3, 0x78, 0x4b, 0x71):
                        # These nominal addresses select unrelated peripherals.
                        # Avoid media writes/faults; still check RIM deselection.
                        code += [0o300] * 5
                    else:
                        code += pointer(bus) + command(0o127, bus ^ 0x55) + [0o123]
                code += command(0o121, mask) + pointer(0)
                for offset in range(256):
                    code += [0o101] + hl(8192 + offset) + [0o370]
                self.load(sim, code)
                expected_pointer = 0
                expected_buffer = [0] * 256
                for bus in range(256):
                    sim.command('run 7')
                    selected = all(bus & (1 << bit) for bit in range(8)
                                   if mask & (1 << bit))
                    if selected:
                        expected_pointer = (bus + 1) & 255
                        expected_buffer[bus] = bus ^ 0x55
                    state = sim.command(f'rim-state {mask}')['rim']
                    self.assertEqual(state['pointer'], expected_pointer,
                                     f'mask={mask:03o} bus={bus:03o}')
                    self.assertFalse(state['data_mode'])
                self.assertTrue(sim.command('run 10000')['halted'])
                self.assertEqual(sim.command('memory 8192 256')['memory'], expected_buffer)

    def test_six_nominal_addresses_and_multi_responder_strobes(self):
        sim = self.simulator()
        for node, mask in enumerate(RESERVED[1:], 2):
            sim.command(f'rim {mask} {node}')
        code = []
        for node, mask in enumerate(RESERVED, 1):
            code += command(0o121, mask) + pointer(5) + command(0o127, node)
        # 233 selects second and third only: every strobe reaches both once.
        code += command(0o121, 0o233) + page(3) + pointer(200)
        code += command(0o127, 0xa5) + command(0o131, 6) + [0o123]
        code += [0o101] + hl(8192) + [0o370]
        code += pointer(200) + [0o103] + hl(8193) + [0o370]  # Agreeing PIN.
        # Nominal selection preserves six distinct buffers, including odd masks.
        for index, mask in enumerate(RESERVED):
            code += command(0o121, mask) + page(0) + pointer(5) + [0o101]
            code += hl(8194 + index) + [0o370]
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 8')['memory'],
                         [TA | DA | RI, 0xa5] + list(range(1, 7)))
        for mask in RESERVED:
            state = sim.command(f'rim-state {mask}')['rim']
            self.assertEqual(bool(state['status'] & POR), mask not in (SECOND, 0o231))
        for command_text in (f'rim-state {0o233}', f'rim-event {0o233} reset'):
            with self.assertRaises(RuntimeError):
                sim.command(command_text)

    def test_multi_responder_input_conflict_consumes_both_buffers(self):
        sim = self.simulator()
        sim.command(f'rim {SECOND} 2')
        code = []
        for mask, value in ((RIM, 0xf3), (SECOND, 0x3f)):
            code += command(0o121, mask) + pointer(7) + command(0o127, value)
        code += command(0o121, 0o236) + pointer(7) + [0o101]
        code += hl(8192) + [0o370]
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 1')['memory'], [0x33])
        for mask in (RIM, SECOND):
            self.assertEqual(sim.command(f'rim-state {mask}')['rim']['pointer'], 8)

    def test_conflicting_pin_dispatches_input_parity_fault(self):
        sim = self.simulator()
        sim.command(f'rim {SECOND} 2')
        code = []
        for mask, value in ((RIM, 0xf3), (SECOND, 0x3f)):
            code += command(0o121, mask) + pointer(7) + command(0o127, value)
        code += command(0o121, 0o236) + pointer(7) + [0o103]
        self.load(sim, code)
        sim.command('run 17')  # Twelve setup + four selection + PIN.
        for mask in (RIM, SECOND):
            self.assertEqual(sim.command(f'rim-state {mask}')['rim']['pointer'], 8)
        self.assertIn('stackptr=0', sim.command('inspect')['debug'])
        sim.command('run 1')
        self.assertIn('stackptr=1', sim.command('inspect')['debug'])

    def test_attachment_waits_for_an_address_strobe(self):
        sim = self.simulator()
        code = command(0o121, 0o235) + pointer(10) + command(0o127, 0xaa)
        code += command(0o121, 0o235) + pointer(20) + command(0o127, 0xbb)
        self.load(sim, code)
        sim.command('run 4')
        sim.command(f'rim {0o231} 2')  # Also matches 235, but has seen no strobe.
        sim.command('run 2')
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['pointer'], 11)
        self.assertEqual(sim.command(f'rim-state {0o231}')['rim']['pointer'], 0)
        self.assertTrue(sim.command('run 1000')['halted'])
        for mask in (RIM, 0o231):
            self.assertEqual(sim.command(f'rim-state {mask}')['rim']['pointer'], 21)

    def test_block_transfers_reach_all_mask_selected_modules(self):
        sim = self.simulator()
        sim.command(f'rim {SECOND} 2')
        data = bytes((index * 19 + 7) & 255 for index in range(16))
        code = command(0o121, 0o236) + page(3) + pointer(247)
        code += hl(4096) + [0o026, 16, 0o111, 0o071]
        code += pointer(247) + hl(8192) + [0o026, 16, 0o111, 0o061]
        self.execute(sim, code, data)
        self.assertEqual(sim.command('memory 8192 16')['memory'], list(data))
        for mask in (RIM, SECOND):
            state = sim.command(f'rim-state {mask}')['rim']
            self.assertEqual((state['processor_page'], state['pointer']), (3, 7))

    def test_original_rimtest_accepts_both_address_checks(self):
        sim = self.simulator()
        sim.command(f'rim {SECOND} 2')
        sim.command('pc 61470')
        sim.command('run 10000000')
        tape = Path(__file__).resolve().parents[2] / 'tapes/diagnostics/RIMTEST.tap'
        sim.load(tape)
        sim.command('run 5000000')
        for character in '1\r2\r234':
            sim.key(character)
            sim.command('run 100000')
        trace = Path(self.work.name) / 'rimtest.trace'
        sim.command(f'trace {trace}')
        sim.key('\r')
        sim.command('run 2000')
        sim.command('trace /dev/null')
        lines = trace.read_text().splitlines()
        probes = [line.split('A=')[1].split()[0] for line in lines
                  if line.startswith('026363 ') and 'EX_ADR' in line]
        self.assertEqual(probes, [f'{RIM ^ (1 << bit):03o}' for bit in range(8)])
        inputs = [line for line in lines if line.startswith('026365 ')]
        self.assertIn('A=261 ', inputs[0])  # Fresh RIM status includes DA.
        self.assertTrue(any(line.startswith('026406 ') for line in lines))
        state = sim.command('run 100000')
        self.assertIn('I/O Address for RIM (Right)', state['screen'])
        for character in '232':
            sim.key(character)
            sim.command('run 100000')
        sim.command(f'trace {trace}')
        sim.key('\r')
        sim.command('run 2000')
        sim.command('trace /dev/null')
        lines = trace.read_text().splitlines()
        probes = [line.split('A=')[1].split()[0] for line in lines
                  if line.startswith('026363 ') and 'EX_ADR' in line]
        self.assertEqual(probes, [f'{SECOND ^ (1 << bit):03o}' for bit in range(8)])
        state = sim.command('run 100000')
        self.assertNotIn('I/O Addressing Failure', state['screen'])
        self.assertIn('I/O Adr: 234', state['screen'])
        self.assertIn('I/O Adr: 232', state['screen'])
        self.assertIn('Phase:', state['screen'])

    def test_commands_keep_page_registers_independent_and_do_not_fake_link(self):
        sim = self.simulator()
        code = command(0o121, RIM) + page(3) + pointer(42)
        code += command(0o131, (2 << 3) | 4) + command(0o131, (1 << 3) | 5)
        # These pages cannot be reselected while the corresponding engine
        # is busy. Processor page remains independently changeable.
        code += command(0o131, 4) + command(0o131, 5) + page(0)
        code += command(0o131, 1) + command(0o131, 2)
        self.execute(sim, code)
        state = sim.command(f'rim-state {RIM}')['rim']
        self.assertEqual((state['processor_page'], state['transmit_page'], state['receive_page']), (0, 2, 1))
        self.assertEqual(state['pointer'], 42)
        self.assertEqual(state['status'], POR | DA)
        self.assertTrue(state['disable_transmit_pending'])
        self.assertTrue(state['disable_receive_pending'])
        # Further CPU time cannot complete a transmission without a link.
        sim.command('pc 0')
        sim.command('run 10000')
        self.assertFalse(sim.command(f'rim-state {RIM}')['rim']['status'] & TMA)

    def test_clear_reset_and_reconfiguration_flags(self):
        sim = self.simulator()
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['status'], TA | POR | DA | RI)
        sim.command(f'rim-event {RIM} recon')
        code = command(0o121, RIM) + command(0o131, 6) + [0o101] + hl(8192) + [0o370]
        code += command(0o131, 7) + [0o101] + hl(8193) + [0o370]
        # Reserved encodings and other COM strobes must not alias commands.
        for opcode, value in ((0o131, 0x86), (0o131, 0o016), (0o133, 6), (0o135, 6)):
            code += command(opcode, value)
        self.execute(sim, code)
        self.assertEqual(sim.command('memory 8192 2')['memory'], [TA | RECON | DA | RI, TA | DA | RI])
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['status'], TA | DA | RI)
        reset = sim.command(f'rim-event {RIM} reset')['rim']
        self.assertEqual(reset['status'], TA | POR | DA | RI)
        self.assertFalse(reset['data_mode'])
        self.assertFalse(reset['disable_transmit_pending'])

    def test_min_mout_counts_registers_flags_and_timing(self):
        # Zero/16 multiples mean 16 bytes per instruction, not a whole 256
        # byte block. Check the full eight-bit decrement and final flags.
        for count in (0, 1, 15, 16, 17, 31, 128, 255):
            for opcode, ns in ((0o061, 8400), (0o071, 8800)):
                with self.subTest(count=count, opcode=oct(opcode)):
                    with Harness() as sim:
                        sim.command('cpu 5500')
                        sim.command(f'rim {RIM} 1')
                        width = count & 15 or 16
                        data = bytes(range(16))
                        code = command(0o121, RIM) + page(1) + pointer(250)
                        # Seed the page using ordinary WRITE, independently
                        # of the block instruction under test.
                        for value in data:
                            code += command(0o127, value if opcode == 0o061 else value + 128)
                        code += pointer(250) + hl(8190 if opcode == 0o061 else 4096)
                        code += [0o026, count]
                        tail = []
                        if opcode == 0o071:
                            tail = pointer(250)
                            for index in range(16):
                                tail += [0o101] + hl(8192 + index) + [0o370]
                        self.load(sim, code + [0o111, opcode] + tail, data)
                        # Count decoded instructions, not prefix bytes.
                        setup_count = 2 + 2 + 2 + 32 + 2 + 2 + 1
                        before = sim.command(f'run {setup_count}')
                        after = sim.command('run 1')
                        self.assertFalse(after['halted'])
                        self.assertEqual(after['time_ns'] - before['time_ns'], width * ns)
                        regs, flags = registers(sim)
                        remaining = (count - width) & 255
                        self.assertEqual(regs[2], remaining)
                        start = 8190 if opcode == 0o061 else 4096
                        self.assertEqual((regs[5] << 8) | regs[6], start + width)
                        # Carry reflects the final decrement, not the initial
                        # borrow when C starts at zero. Parity flag is odd.
                        self.assertEqual(flags, (0, int(remaining == 0), int(bool(remaining & 128)),
                                                 remaining.bit_count() % 2))
                        state = sim.command(f'rim-state {RIM}')['rim']
                        self.assertEqual(state['pointer'], (250 + width) & 255)
                        if opcode == 0o061:
                            self.assertEqual(sim.command(f'memory 8190 {width}')['memory'], list(data[:width]))
                        else:
                            self.assertTrue(sim.command('run 10000')['halted'])
                            expected = list(data[:width]) + [value + 128 for value in data[width:]]
                            self.assertEqual(sim.command('memory 8192 16')['memory'], expected)

    def test_full_page_min_mout_loop(self):
        sim = self.simulator()
        code = command(0o121, RIM) + page(3) + pointer(0) + hl(4096) + [0o026, 0]
        start = len(code)
        code += [0o111, 0o071, 0o110, start & 255, start >> 8]
        code += pointer(0) + hl(8192) + [0o026, 0]
        start = len(code)
        code += [0o111, 0o061, 0o110, start & 255, start >> 8]
        self.execute(sim, code, bytes(range(256)))
        self.assertEqual(sim.command('memory 8192 256')['memory'], list(range(256)))
        regs, flags = registers(sim)
        self.assertEqual((regs[5] << 8) | regs[6], 8192 + 256)
        self.assertEqual(regs[2], 0)
        self.assertEqual(flags, (0, 1, 0, 0))
        self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['pointer'], 0)

    def test_min_mout_user_mode_has_no_io_side_effects(self):
        for opcode in (0o061, 0o071):
            with self.subTest(opcode=oct(opcode)), Harness() as sim:
                sim.command('cpu 5500')
                sim.command(f'rim {RIM} 1')
                code = command(0o121, RIM) + pointer(7) + hl(8192) + [0o026, 4]
                # CALL a USER RETURN helper to enter user mode at the block
                # instruction. Stop before dispatching the fault's ROM vector.
                helper = len(code) + 6
                code += [0o106, helper & 255, helper >> 8, 0o111, opcode, 0, 0o111, 0o102]
                self.load(sim, code)
                sim.command('run 9')  # Seven setup instructions, CALL and UR.
                state = sim.command('run 1')
                self.assertFalse(state['halted'])
                self.assertEqual(sim.command(f'rim-state {RIM}')['rim']['pointer'], 7)
                regs, _ = registers(sim)
                self.assertEqual(regs[2], 4)
                self.assertEqual((regs[5] << 8) | regs[6], 8192)
                self.assertEqual(sim.command('memory 8192 4')['memory'], [0] * 4)
                # The next step must dispatch the privilege fault, pushing
                # the faulting block instruction on the CPU stack.
                sim.command('run 1')
                debug = sim.command('inspect')['debug']
                self.assertIn('stackptr=1', debug)


if __name__ == '__main__':
    unittest.main()
