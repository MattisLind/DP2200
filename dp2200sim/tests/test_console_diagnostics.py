"""Regression checks driven by the original 2200 memory/console diagnostics."""
from pathlib import Path
import re
import unittest

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]
CHARACTERS = ''.join(chr(code) for code in range(32, 112))
CHARACTER_TAIL = ''.join(chr(code) for code in range(112, 127)).ljust(80)
KEYBOARD_ROWS = (
    ('TYPE THE TOP OR FIRST ROW OF KEYS INCLUDING BACKSPACE.', '1234567890-_[\x08'),
    ('TYPE THE TOP ROW SHIFTED.', '!"#$%&\'() =\x7f{\x08'),
    ('TYPE THE SECOND ROW STARTING WITH CANCEL.', '\x18qwertyuiop`\\^'),
    ('TYPE THE SECOND ROW SHIFTED.', '\x18QWERTYUIOP@|~'),
    ('TYPE THE THIRD ROW.', 'asdfghjkl;:]'),
    ('TYPE THE THIRD ROW SHIFTED.', 'ASDFGHJKL+*}'),
    ('TYPE THE FOURTH ROW.', 'zxcvbnm,./'),
    ('TYPE THE FOURTH ROW SHIFTED.', 'ZXCVBNM<>?'),
    ('TYPE THE SPACE BAR 3 TIMES.', '   '),
    ("TYPE THE NUMBERS IN THE 10 KEY GROUP IN NUMERICAL ORDER STARTING WITH '.0'.",
     '.0123456789'),
)


class ConsoleDiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.sim = Harness()
        self.addCleanup(self.sim.__exit__)

    def execute(self, count=100000):
        state = self.sim.command(f'run {count}', timeout=30)
        self.assertFalse(state['halted'], state['screen'])
        return state

    def boot(self, name):
        self.sim.load(ROOT / f'tapes/diagnostics/{name}.tap')
        return self.execute(5000000)

    def native_keys(self, text):
        # Expected rows are native bytes, including CANCEL, DEL and BACKSPACE.
        # Harness.key() instead translates host letters and DEL for the UI.
        for character in text:
            self.sim.command(f'key {ord(character)}')
            state = self.execute(10000)
            self.assertFalse(state['keyboard_ready'], f'Unconsumed key {character!r}')
        return self.execute()

    def test_pcmtest_completes_memory_pass_without_errors(self):
        self.sim.load(ROOT / 'tapes/diagnostics/PCMTEST_V1.1.tap')
        previous = None
        positions = set()
        for _ in range(12):  # 60 million instructions, no wall-clock polling.
            state = self.execute(5000000)
            screen = state['screen']
            self.assertIn('PATHOLOGICAL CASE MEMORY TEST PROGRAM VERSION 1.1', screen)
            progress = re.search(r'16K MACHINE\s+PASS\s+(\d+)\s+ROW\s+(\d+)', screen)
            self.assertIsNotNone(progress, screen)
            position = tuple(map(int, progress.groups()))
            self.assertGreaterEqual(position[0], 1)
            self.assertGreaterEqual(position[1], 1)
            if previous is not None:
                self.assertGreaterEqual(position, previous, 'Memory test moved backwards')
            previous = position
            positions.add(position)
            self.assertIn('NUMBER OF ERRORS IN EACH 1K RAM', screen)
            grid = [line.split() for line in screen.splitlines()
                    if re.fullmatch(r'\s*[0-7](?:\s+\d+){16}\s*', line)]
            self.assertEqual([int(row[0]) for row in grid], list(range(7, -1, -1)), screen)
            self.assertTrue(all(int(counter) == 0 for row in grid for counter in row[1:]),
                            f'Memory diagnostic reported errors:\n{screen}')
            if position[0] >= 2:
                self.assertGreater(len(positions), 1, 'No observed row progression')
                return
        self.fail(f'PCMTEST did not complete a pass within its budget:\n{screen}')

    def display_to(self, text):
        # Release as soon as the new phase label appears. Holding DISPLAY for
        # a long batch can skip several tests because this is a level input.
        self.sim.command('button display 1')
        try:
            for _ in range(10000):
                state = self.execute(10)
                if text in state['screen']:
                    break
            else:
                self.fail(f'DISPLAY did not reach {text!r}:\n{state["screen"]}')
        finally:
            self.sim.command('button display 0')
        return self.execute()

    def assert_controls(self, state, cursor=False, keyboard=False, display=False):
        self.assertEqual(state['cursor_visible'], cursor, state['screen'])
        self.assertEqual(state['keyboard_light'], keyboard, state['screen'])
        self.assertEqual(state['display_light'], display, state['screen'])

    def test_tstdis_patterns_erase_cursor_and_lights(self):
        state = self.boot('TSTDIS')
        self.assertIn('Depress the DISPLAY key to begin the test.', state['screen'])
        state = self.display_to(' !')
        self.assertEqual(state['screen'].splitlines(), [CHARACTERS, CHARACTER_TAIL] * 6)
        self.assert_controls(state, cursor=True)
        state = self.display_to('@?')
        # The rolling phase is continuously writing the last row. Completed
        # rows must be full-width and the partial last row must retain phase.
        for _ in range(3):
            rows = state['screen'].splitlines()
            self.assertEqual(rows[:11], ['@?' * 40] * 11)
            self.assertRegex(rows[11], r'^(?:@\?)*(?:@)? *$')
            state = self.execute()
        state = self.display_to('ERASE END OF LINE')
        expected = ['ERASE END OF LINE'.ljust(80)] + [
            CHARACTERS if row in (1, 3, 5) else ' ' * 80 for row in range(1, 12)]
        self.assertEqual(state['screen'].splitlines(), expected)
        self.assert_controls(state)
        labels = ['ERASE END OF LINE', 'ERASE END OF FRAME']
        state = self.display_to(labels[-1])
        self.assertEqual(state['screen'].splitlines(),
                         [label.ljust(80) for label in labels] + [' ' * 80] * 10)
        # TSTDIS enables the cursor after erasing the frame, before the
        # subsequent DISPLAY press prints the CURSOR ON label.
        self.assert_controls(state, cursor=True)
        for label, cursor, keyboard, display in (
                ('CURSOR ON', True, False, False),
                ('CURSOR OFF', False, False, False),
                ('KEYBOARD LIGHT ON', False, True, False),
                ('DISPLAY LIGHT ON', False, False, True)):
            labels.append(label)
            state = self.display_to(label)
            self.assertEqual(state['screen'].splitlines(),
                             [item.ljust(80) for item in labels] + [' ' * 80] * (12 - len(labels)))
            self.assert_controls(state, cursor, keyboard, display)
        state = self.display_to('2200 DISPLAY TEST')
        self.assertIn('Depress the DISPLAY key to begin the test.', state['screen'])
        self.assert_controls(state, cursor=True)

    def start_keyboard(self):
        state = self.boot('TSTKEY')
        self.assertEqual(state['screen'].strip(), 'OLD OR NEW KEYBOARD?')
        self.sim.key('N')
        state = self.execute()
        self.assertIn('2200 KEYBOARD TEST', state['screen'])
        self.assertIn('Depress the ENTER key when you are ready to begin.', state['screen'])
        self.sim.key('\n')
        return self.execute()

    def test_tstkey_new_keyboard_rows_and_keypad(self):
        state = self.start_keyboard()
        beeps = state['beeps']
        for prompt, row in KEYBOARD_ROWS:
            with self.subTest(prompt=prompt):
                self.assertIn(prompt, state['screen'])
                # Validate independently specified expected bytes before
                # typing them; never echo whatever the simulator displays.
                self.assertIn(row.ljust(80), state['screen'].splitlines())
                state = self.native_keys(row + '\r')
                self.assertEqual(state['beeps'], beeps, state['screen'])
        self.assertIn('DEPRESS THE KEYBOARD KEY.', state['screen'])

    def test_tstkey_wrong_key_beeps_and_short_line_retries(self):
        state = self.start_keyboard()
        beeps = state['beeps']
        state = self.native_keys('X')
        self.assertEqual(state['beeps'], beeps + 1)
        state = self.native_keys('\r')
        self.assertIn(KEYBOARD_ROWS[0][0], state['screen'])
        self.assertNotIn(KEYBOARD_ROWS[1][0], state['screen'])
        self.assertEqual(state['screen'].splitlines()[2], ' ' * 80)
        state = self.native_keys(KEYBOARD_ROWS[0][1] + '\r')
        self.assertIn(KEYBOARD_ROWS[1][0], state['screen'])
        self.assertEqual(state['beeps'], beeps + 1)


if __name__ == '__main__':
    unittest.main()
