"""Exercise screen and keyboard I/O with small machine programs and the real tape."""
from pathlib import Path
import struct
import tempfile
import unittest
from harness import Harness

ROOT = Path(__file__).resolve().parents[2]


def la(value):
    return bytes([0o006, value])


def command(value, opcode):
    return la(value) + bytes([opcode])


SELECT_SCREEN = command(0xe1, 0o121)
HALT = bytes([0])


class HeadlessTests(unittest.TestCase):
    def program(self, body):
        # SIMH tape framing used by CassetteTape::loadBoot.
        tape = Path(self.directory.name) / "fixture.tap"
        record = SELECT_SCREEN + body + HALT
        size = struct.pack("<I", len(record))
        tape.write_bytes(size + record + size)
        self.sim.load(tape)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.sim = Harness()

    def tearDown(self):
        self.sim.__exit__()
        self.directory.cleanup()

    def test_keyboard_latch_consumed_by_cpu(self):
        self.program(bytes([0o125, 0o101, 0o127]))  # EX_DATA, INPUT, EX_WRITE
        self.sim.key("a")  # Interactive keyboard translates lowercase a to native A.
        with self.assertRaisesRegex(RuntimeError, "occupied"):
            self.sim.key("b")
        state = self.sim.run_until("A")
        self.assertFalse(state["keyboard_ready"])
        self.assertEqual(state["screen"].splitlines()[0], "A" + " "*79)

    def test_buttons_are_visible_to_cpu(self):
        # INPUT status, add ASCII '0', then write the resulting byte to screen.
        self.program(bytes([0o101, 0o004, ord("0"), 0o127]))
        self.sim.command("button display 1")
        self.sim.command("button keyboard 1")
        self.sim.run_until("M")  # RAM_DISPLAY | CRT_READY | KEYBOARD | DISPLAY = 29.

    def test_programmable_font_loading_and_wrap(self):
        # EX COM4 selects glyph 127; the sixth write wraps to glyph zero.
        body = command(127, 0o137)
        for value in (0o100, 0o200, 0, 0, 1, 0o177):
            body += command(value, 0o127)
        body += command(0, 0o131) + command(ord('A'), 0o127)
        self.program(body)
        self.sim.run_until('A')
        font = self.sim.command('font-state')['font']
        self.assertEqual(font['glyphs'][127], [0o100, 0, 0, 0, 1])
        self.assertEqual(font['glyphs'][0], [0o177, 0, 0, 0, 0])
        self.assertEqual(font['writes'], 6)
        self.assertEqual(font['glyph_writes'][127], 5)
        self.assertEqual(font['glyph_writes'][0], 1)

    def test_beep_count_is_reported(self):
        self.program(bytes([0o151, 0o151]))  # EX_BEEP twice.
        self.assertEqual(self.sim.command('state')['beeps'], 0)
        self.assertEqual(self.sim.command('run 100')['beeps'], 2)
        self.assertEqual(self.sim.command('state')['beeps'], 2)

    def test_screen_scroll_erase_and_lights(self):
        body = command(ord("A"), 0o127)
        body += command(1, 0o135) + command(ord("B"), 0o127)
        body += command(8, 0o131)  # Roll up: B must survive in row 0.
        body += command(1, 0o131)  # Roll down: B must survive in row 1.
        body += command(2, 0o133) + command(0, 0o135)
        body += command(0xe0, 0o131)  # Both lights and auto-increment.
        body += command(ord("C"), 0o127) + command(ord("D"), 0o127)
        body += command(3, 0o133) + command(0x62, 0o131)  # Erase D, keep lights.
        self.program(body)
        state = self.sim.run_until("C")
        rows = state["screen"].splitlines()
        self.assertEqual(rows[0], "  C" + " "*77)
        self.assertEqual(rows[1], "B" + " "*79)
        self.assertTrue(state["keyboard_light"])
        self.assertTrue(state["display_light"])

    def test_cassette_callbacks_use_simulated_time(self):
        # Stop makes deck ready; RBK reads the record after the bootstrap.
        record = command(0xf0, 0o121) + bytes([0o177, 0o161])
        loop = len(record)
        record += bytes([0o101, 0o044, 4, 0o150, loop & 255, loop >> 8])
        record += bytes([0o125, 0o101, 0o310])  # Read byte into B.
        record += SELECT_SCREEN + bytes([0o301, 0o127]) + HALT
        tape = Path(self.directory.name) / "cassette.tap"
        def framed(data):
            size = struct.pack("<I", len(data))
            return size + data + size
        tape.write_bytes(framed(record) + framed(b"Z"))
        self.sim.load(tape)
        state = self.sim.run_until("Z", batch=10000)
        self.assertGreaterEqual(state["time_ns"], 72_800_000)

    def test_5500_rom_debugger_memory_commands(self):
        self.sim.command('cpu 5500')
        # Run the real ROM's initialization, then enter its restart monitor
        # with DISPLAY held, as the interactive AUTORESTART path does.
        self.sim.command(f'pc {0o170036}')
        self.assertTrue(self.sim.command('run 10000000')['halted'])
        self.sim.command('button display 1')
        self.sim.command(f'pc {0o175724}')
        self.sim.command('run 100000')
        self.sim.command('button display 0')

        def enter(text):
            for character in text:
                self.sim.key(character)
                state = self.sim.command('run 100000')
                self.assertFalse(state['halted'])
                self.assertFalse(state['keyboard_ready'])
            return state['screen'].split('\n')

        rows = enter('170036\n')
        self.assertEqual(rows[8], ' '*74 + '170036')
        self.assertEqual(rows[9], ' '*77 + '040')
        self.assertEqual(rows[10], ' '*74 + '043040')
        rows = enter('100\n101M')
        self.assertEqual(rows[8], ' '*74 + '000100')
        self.assertEqual(rows[9], ' '*74 + 'A  101')
        self.assertEqual(rows[10], ' '*74 + '000101')
        self.assertEqual(enter('I')[8], ' '*74 + '000101')
        self.assertEqual(enter('D')[8], ' '*74 + '000100')

    def test_5500_cold_rom_boot_dos_c(self):
        self.sim.command('cpu 5500')
        self.sim.command(f'floppy 0 {ROOT / "DOS.C/003.IMD"}')
        self.sim.command(f'pc {0o170036}')
        self.assertTrue(self.sim.command('run 1000000')['halted'])
        # Match the interactive runner's automatic restart after ROM power-up.
        self.sim.command(f'pc {0o175724}')
        state = self.sim.command('run 3000000')
        self.assertNotIn('ACCESS PROTECT', state['screen'])
        self.assertIn('DOS.C 2.4', state['screen'])
        self.assertIn('READY', state['screen'])
        self.assertFalse(state['halted'])

    def test_wrong_success_text_fails(self):
        self.program(command(ord("X"), 0o127))
        with self.assertRaisesRegex(AssertionError, "halted before"):
            self.sim.run_until("TEST COMPLETED")

    def test_tstpro(self):
        self.sim.load(ROOT / "tapes/diagnostics/tstpro1.1.tap")
        state = self.sim.run_until("TEST COMPLETED")
        self.assertEqual(state["screen"].strip(), "TEST COMPLETED")
        self.assertTrue(state["halted"])
        print(f"TSTPRO: TEST COMPLETED; halt PC={state['pc']:05o}, "
              f"instructions={state['instructions']}, simulated ns={state['time_ns']}")

    def test_test5556_instructions(self):
        self.sim.command("cpu 5500")
        # The power-up ROM installs interrupt vectors in system RAM. Loading
        # the cassette bootstrap alone does not perform that initialization.
        self.sim.command(f"pc {0o170036}")
        self.assertTrue(self.sim.command("run 10000000")["halted"])
        self.sim.load(ROOT / "tapes/diagnostics/DIAG5500_V1.1.tap")
        state = self.sim.command("run 2000000")
        self.assertIn("TEST5556", state["screen"])
        self.sim.key("3")
        self.sim.command("run 100000")
        self.sim.key("\n")
        state = self.sim.command("run 25000000", timeout=30)
        menu = "5556 Processor Test 1.1"
        self.assertIn(menu, state["screen"])
        # Uppercase host input becomes lowercase native diagnostic commands.
        self.sim.key("E")
        state = self.sim.command("run 500000")
        self.assertIn('TEST 5556 IO IN C  TYPE IN A "D".', state["screen"])
        self.sim.key("D")
        saw_bcv = False
        for _ in range(60):
            state = self.sim.command("run 1000000")
            self.assertFalse(state["halted"], state["screen"])
            self.assertNotIn("ERROR", state["screen"])
            saw_bcv |= "TEST 5556 BCV" in state["screen"]
            if menu in state["screen"]:
                self.assertTrue(saw_bcv, "returned to menu before final BCV test")
                print("TEST5556: instruction option E completed and returned to menu")
                return
        self.fail(f"TEST5556 instruction budget exhausted:\n{state['screen']}")

    def test_diag6600_boot_menu(self):
        # A missing file-1 tape header previously made UNITEST overwrite this
        # menu. This checks cassette loading, not the 6600 instruction suite.
        self.sim.command("cpu 6600")
        self.sim.command(f"pc {0o170036}")
        self.sim.command("run 10000000")
        self.sim.load(ROOT / "tapes/diagnostics/DIAG6600_V1.1.tap")
        state = self.sim.command("run 2000000")
        self.assertFalse(state["halted"])
        for label in ("DIAG6600", "TST6600C", "TST6600D", "LOAD FILE NUMBER?"):
            self.assertIn(label, state["screen"])


if __name__ == "__main__":
    unittest.main()
