"""Real cassette diagnostics plus boundary cases their transport probes expose."""
from pathlib import Path
import tempfile
import unittest

from cassette_diagnostics import framed, run_exrcass, run_taptim
from harness import Harness


class CassetteDiagnosticTests(unittest.TestCase):
    def test_exrcass_both_decks_transport(self):
        self.assertTrue(run_exrcass()['functional_pass'])

    def test_taptim_both_decks_write_read_loop(self):
        # Verify diagnostic execution and recorded patterns. Timing tolerances
        # are reported separately by --require-timing, not asserted as passed.
        for deck in (0, 1):
            with self.subTest(deck=deck):
                result = run_taptim(deck)
                self.assertIn(600, result['record_lengths'])
                self.assertIn(100, result['record_lengths'])
                self.assertEqual(len(result['measurements']), 7)

    def test_taptim_bounded_endurance_both_decks(self):
        for deck in (0, 1):
            with self.subTest(deck=deck):
                result = run_taptim(deck, 'E')
                self.assertTrue(result['bounded_data_pass'])
                self.assertGreaterEqual(result['pattern_bytes_read'], 1000)

    def native(self, tape, operations):
        with tempfile.TemporaryDirectory() as work, Harness() as sim:
            media = Path(work) / 'data.tap'
            media.write_bytes(tape)
            sim.command(f'tape 1 {media}')
            code = bytearray([0o006, 0xf0, 0o121, 0o157, 0o177])

            def wait(mask):
                code.append(0o123)  # EX_STATUS
                loop = len(code)
                code.extend([0o101, 0o044, mask, 0o150, loop & 255, loop >> 8])

            def store(slot):
                code.extend([0o056, 32, 0o066, slot, 0o370])

            for slot, operation in enumerate(operations):
                code.append(operation)
                if tape:
                    wait(4)
                    code.extend([0o125, 0o101])  # Consume one byte.
                    store(slot)
                    wait(1)  # Finish this record before the next operation.
                else:
                    wait(1)
                    code.extend([0o123, 0o101])
                    store(slot)
            code.append(0)
            boot = Path(work) / 'boot.tap'
            boot.write_bytes(framed(code))
            sim.load(boot)
            state = sim.command('run 1000000')
            self.assertTrue(state['halted'], state['screen'])
            values = sim.command(f'memory 8192 {len(operations)}')['memory']
            cassette = sim.command('cassette-state')['cassette']
            self.assertFalse(cassette['running'])
            self.assertEqual(media.read_bytes(), tape)
            return values, cassette

    def test_one_byte_records_forward_and_reverse(self):
        values, state = self.native(framed(b'\x96') + framed(b'\x35'),
                                    [0o161, 0o161, 0o167, 0o167])
        self.assertEqual(values, [0x96, 0x35, 0xac, 0x69])
        self.assertEqual(state['decks'][1]['position'], 0)
        self.assertEqual(state['status'] & 3, 3)

    def test_empty_tape_read_finishes_without_data(self):
        values, state = self.native(b'', [0o161])
        self.assertEqual(values[0] & (1 | 2 | 4 | 64), 1 | 2 | 64)
        self.assertEqual(state['decks'][1]['position'], 0)


if __name__ == '__main__':
    unittest.main()
