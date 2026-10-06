"""Line-oriented client for the real simulator's headless console."""
import json
from pathlib import Path
import select
import subprocess

SIMULATOR = Path(__file__).resolve().parents[1] / "dp2200sim-headless"


class Harness:
    def __init__(self, executable=SIMULATOR):
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1,
        )

    def command(self, command, timeout=10):
        if "\n" in command or "\r" in command:
            raise ValueError("commands must fit on one line")
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], timeout)[0]:
            raise TimeoutError(f"simulator did not answer {command!r}")
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(f"simulator exited with {self.process.poll()}")
        state = json.loads(line)
        if not state["ok"]:
            raise RuntimeError(state["error"])
        return state

    def load(self, tape):
        return self.command(f"load {Path(tape).resolve()}")

    def key(self, character):
        """Match the interactive UI's encoding; key command itself takes native bytes."""
        if len(character) != 1:
            raise ValueError("send one character at a time, after the latch is consumed")
        code = ord(character)
        if character == "\n":
            code = 13
        elif code == 127:
            code = 8
        elif code == 27:
            code = 0x30
        elif character.isascii() and character.isalpha():
            code ^= 0x20
        return self.command(f"key {code}")

    def run_until(self, text, max_instructions=1_000_000, batch=1000):
        """Require both the expected visible text and a CPU halt within the budget."""
        remaining = max_instructions
        while remaining > 0:
            count = min(batch, remaining)
            state = self.command(f"run {count}")
            remaining -= count
            if state["halted"]:
                if text not in state["screen"]:
                    raise AssertionError(f"CPU halted before {text!r}:\n{state['screen']}")
                return state
        raise AssertionError(f"instruction budget exhausted waiting for {text!r}:\n{state['screen']}")

    def __enter__(self):
        return self

    def __exit__(self, *_):
        try:
            if self.process.poll() is None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
            self.process.wait(timeout=2)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait()
        finally:
            self.process.stdin.close()
            self.process.stdout.close()
