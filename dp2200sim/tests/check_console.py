"""Run the curses display regression with a sufficiently large pseudo-terminal."""
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import termios
import time
import sys

master, slave = pty.openpty()
fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 50, 200, 0, 0))
env = dict(os.environ, TERM='xterm')
executable = Path(__file__).resolve().parents[1] / (
    '.console-test/check_console-sdl' if '--sdl' in sys.argv else '.console-test/check_console')
if '--sdl' in sys.argv:
    env.update(SDL_VIDEODRIVER='cocoa' if '--window' in sys.argv else 'dummy', SDL_RENDER_DRIVER='software')
proc = subprocess.Popen([str(executable)], stdin=slave, stdout=slave, stderr=slave,
                        env=env, preexec_fn=lambda: signal.pthread_sigmask(signal.SIG_SETMASK, []))
os.close(slave)
output = bytearray()
try:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if select.select([master], [], [], .1)[0]:
            try:
                output.extend(os.read(master, 65536))
            except OSError:
                break
        elif proc.poll() is not None:
            break
    assert proc.wait(timeout=1) == 0, output.decode(errors='replace')
    assert b'PASS:' in output, output.decode(errors='replace')
    print(output[output.index(b'PASS:'):].decode().strip())
finally:
    if proc.poll() is None:
        proc.kill()
        proc.wait()
    os.close(master)
