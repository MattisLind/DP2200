"""Exercise cassette writing and rereading with native 2200 I/O instructions."""
from pathlib import Path
import struct
import tempfile
import unittest
from harness import Harness

class CassetteWriteTests(unittest.TestCase):
    def test_write_block_then_rewind_and_read_back(self):
        self.write_and_read_back(explicit_stop=True)

    def test_write_block_stops_when_no_next_byte_is_supplied(self):
        self.write_and_read_back(explicit_stop=False)

    def test_rewriting_shorter_record_removes_old_suffix(self):
        self.write_and_read_back(explicit_stop=True, rewrite=True)

    def write_and_read_back(self, explicit_stop, rewrite=False):
        with tempfile.TemporaryDirectory() as work, Harness() as sim:
            output=Path(work)/'output.tap'
            sim.command(f'tape-create 1 {output}')
            # Select cassette, front deck, STOP (ready), WBK.
            code=bytearray([0o006,0xf0,0o121,0o157,0o177,0o173,0o163])
            def wait(mask):
                code.append(0o123)
                loop=len(code)
                code.extend([0o101,0o044,mask,0o150,loop&255,loop>>8])
            for byte in b'BOOT':
                wait(8)
                code.extend([0o006,byte,0o127])
            if explicit_stop:
                code.append(0o177)  # Stop (flush).
            else:
                wait(1)  # Hardware completes a record after the last byte.
            code.append(0o175)  # Rewind.
            wait(1)
            if rewrite:
                code.append(0o163)  # Replace BOOT with one byte at BOT.
                wait(8)
                code.extend([0o006, ord('Z'), 0o127, 0o177, 0o175])
                wait(1)
            code.append(0o161)  # RBK.
            wait(4)
            code.extend([0o125,0o101,0o056,32,0o066,0,0o370,0])
            size=struct.pack('<I',len(code))
            bootstrap=Path(work)/'program.tap'
            bootstrap.write_bytes(size+code+size)
            sim.load(bootstrap)
            state=sim.command('run 1000000')
            self.assertTrue(state['halted'])
            expected = b'Z' if rewrite else b'BOOT'
            self.assertEqual(sim.command('memory 8192 1')['memory'],[expected[0]])
            size = struct.pack('<I', len(expected))
            self.assertEqual(output.read_bytes(),size+expected+size)

    def test_tape_create_preserves_existing_file(self):
        with tempfile.TemporaryDirectory() as work, Harness() as sim:
            output=Path(work)/'existing.tap'
            output.write_bytes(b'preserve')
            with self.assertRaises(RuntimeError):sim.command(f'tape-create 1 {output}')
            self.assertEqual(output.read_bytes(),b'preserve')

if __name__=='__main__':unittest.main()
