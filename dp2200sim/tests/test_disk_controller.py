"""Check disk side effects and asynchronous I/O using real CPU instructions."""
from pathlib import Path
import struct
import tempfile
import unittest

from harness import Harness


class Program:
    def __init__(self):
        self.code = bytearray([0o006, 0x4b, 0o121])  # LA disk address; EX ADR.

    def command(self, value, opcode):
        self.code.extend([0o006, value, opcode])
        return self

    def select(self, argument, command):
        return self.command(argument, 0o133).command(command, 0o131)

    def fill(self, page, data):
        self.command(page, 0o135).command(0, 0o137)
        for value in data:
            self.command(value, 0o127)
        return self

    def wait_ready(self):
        self.code.append(0o123)  # EX STATUS.
        start = len(self.code)
        self.code.extend([0o101, 0o044, 6, 0o110, start & 255, start >> 8])
        return self

    def save_input(self, address):
        self.code.extend([0o101, 0o056, address >> 8, 0o066, address & 255, 0o370])
        return self


class DiskControllerTests(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.root = Path(self.work.name)
        self.sim = Harness()

    def tearDown(self):
        self.sim.__exit__()
        self.work.cleanup()

    def attach(self, drive=0, protected=False):
        path = self.root / f"{drive}.dsk"
        self.sim.command(f"disk {drive} {int(protected)} {path}")
        return path

    def execute(self, program):
        data = bytes(program.code) + b"\0"
        size = struct.pack("<I", len(data))
        tape = self.root / "program.tap"
        tape.write_bytes(size + data + size)
        self.sim.load(tape)
        state = self.sim.command("run 1000000")
        self.assertTrue(state["halted"], "machine program failed to finish")
        return self.sim.command("disk-state")["disk"]

    def memory(self, address, count=1):
        return bytes(self.sim.command(f"memory {address} {count}")["memory"])

    def test_read_offline_drive_sets_error_without_crashing(self):
        program = Program().command(1, 0o131).wait_ready().save_input(8192)
        stats = self.execute(program)
        self.assertEqual(self.memory(8192)[0] & 0x41, 0x40)  # SNF, offline.
        self.assertEqual(stats["errors"], 1)

    def test_transfer_keeps_original_drive_and_page(self):
        disk0, disk1 = self.attach(0), self.attach(1)
        self.assertEqual(disk0.stat().st_size, 24944640)
        pattern = bytes(range(256))
        program = Program().fill(3, pattern).command(2, 0o131)
        # Change selection while the write is outstanding. Its completion
        # must use drive 0/page 3, even though the CPU now selects drive 1/page 7.
        program.select(1, 5).fill(7, b"Z" * 256).wait_ready()
        program.select(0, 5).command(4, 0o135).command(1, 0o131).wait_ready()
        program.command(0, 0o137).code.append(0o125)  # EX DATA.
        for offset in range(256):
            program.save_input(8192 + offset)
        stats = self.execute(program)
        self.assertEqual(self.memory(8192, 256), pattern)
        with disk0.open("rb") as image:
            self.assertEqual(image.read(256), pattern)
        with disk1.open("rb") as image:
            self.assertEqual(image.read(256), bytes(256))
        self.assertEqual((stats["writes"], stats["reads"], stats["errors"]), (1, 1, 0))

    def test_master_clear_cancels_pending_write(self):
        disk = self.attach()
        program = Program().fill(3, b"X" * 256).command(2, 0o131).command(0, 0o131)
        program.code.extend([0o300] * 2000)  # LAA: advance past the canceled deadline.
        program.save_input(8192)
        stats = self.execute(program)
        with disk.open("rb") as image:
            self.assertEqual(image.read(256), bytes(256))
        self.assertEqual(self.memory(8192)[0] & 6, 0)
        self.assertEqual(stats["writes"], 0)

    def test_write_protected_media_stays_unchanged(self):
        disk = self.root / "0.dsk"
        with disk.open("wb") as image:
            image.write(b"Q" * 256)
            image.truncate(24944640)
        self.attach(protected=True)
        stats = self.execute(Program().fill(0, b"X" * 256).command(2, 0o131)
                             .wait_ready().save_input(8192))
        self.assertEqual(self.memory(8192)[0] & 0x21, 0x21)
        with disk.open("rb") as image:
            self.assertEqual(image.read(256), b"Q" * 256)
        self.assertEqual(stats["errors"], 1)

    def test_invalid_sector_does_not_write_previous_sector(self):
        disk = self.root / "0.dsk"
        with disk.open("wb") as image:
            image.write(b"Q" * 256)
            image.truncate(24944640)
        self.attach()
        stats = self.execute(Program().select(99, 10).fill(0, b"X" * 256)
                             .command(2, 0o131).wait_ready().save_input(8192))
        self.assertEqual(self.memory(8192)[0] & 0x40, 0x40)
        with disk.open("rb") as image:
            self.assertEqual(image.read(256), b"Q" * 256)
        self.assertEqual(stats["errors"], 1)

    def test_9374_upper_logical_unit_and_last_sector(self):
        self.sim.command("disk-model 9374")
        disk1, disk9 = self.attach(1), self.attach(9)
        self.assertEqual(disk9.stat().st_size, 10027008)
        pattern = bytes(range(256))
        program = Program().select(4, 5).select(203, 6).select(15, 9).select(23, 10)
        stats = self.execute(program.fill(0, pattern).command(2, 0o131).wait_ready())
        with disk9.open("rb") as image:
            image.seek(-256, 2)
            self.assertEqual(image.read(), pattern)
        with disk1.open("rb") as image:
            image.seek(-256, 2)
            self.assertEqual(image.read(), bytes(256))
        self.assertEqual(stats["errors"], 0)

    def test_9374_format_covers_both_halves_and_preserves_buffer(self):
        self.sim.command("disk-model 9374")
        disk = self.root / "0.dsk"
        with disk.open("wb") as image:
            image.write(b"Q" * (4 * 24 * 256))
            image.truncate(10027008)
        self.attach()
        program = Program().fill(3, b"X" * 256).command(8, 0o131).wait_ready()
        program.command(0, 0o137).code.append(0o125)
        program.save_input(8192)
        stats = self.execute(program)
        self.assertEqual(self.memory(8192), b"X")
        with disk.open("rb") as image:
            tracks = [image.read(24 * 256) for _ in range(4)]
        self.assertEqual(tracks, [b"\xff" * (24 * 256), b"Q" * (24 * 256)] * 2)
        self.assertEqual((stats["formats"], stats["errors"]), (1, 0))

    def test_9374_head_bit_selects_fixed_pack_and_its_protection(self):
        self.sim.command("disk-model 9374")
        removable, fixed = self.attach(0), self.attach(1)
        self.sim.command("disk-protect 1 1")
        program = Program().select(0, 5).select(8, 9)
        program.code.append(0o123)  # EX STATUS.
        program.save_input(8192).select(0, 9).save_input(8193)
        self.execute(program)
        self.assertEqual(self.memory(8192)[0] & 0x21, 0x21)
        self.assertEqual(self.memory(8193)[0] & 0x21, 0x01)
        self.assertEqual(removable.stat().st_size, fixed.stat().st_size)

    def test_9370_identification_matches_reference(self):
        self.execute(Program().command(7, 0o131).save_input(8192))
        self.assertEqual(self.memory(8192), b"\x01")
