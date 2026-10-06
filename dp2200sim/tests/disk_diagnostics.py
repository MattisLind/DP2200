"""Run the supplied 9370/9374 diagnostics using disposable disk images."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from harness import Harness

ROOT = Path(__file__).resolve().parents[2]
TAPES = ("MD370_V1.1", "MD370_V1.2", "MD374_V1.1", "MA374_V1.2.B", "SV374_V1.1")


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class Session:
    def __init__(self, sim):
        self.sim = sim
        self.history = []
        self.state = None

    def run(self, count=100000):
        self.state = self.sim.command(f"run {count}", timeout=30)
        require(not self.state["halted"], f"Unexpected CPU halt:\n{self.state['screen']}")
        if not self.history or self.history[-1]["screen"] != self.state["screen"]:
            self.history.append({k: self.state[k] for k in ("instructions", "pc", "screen")})
        return self.state

    def keys(self, text):
        for character in text:
            self.sim.key(character)
            self.run()

    def keyboard_button(self):
        self.sim.command("button keyboard 1")
        # Some routines only sample KEYBOARD at a buffer/seek pass boundary.
        self.run(1000000)
        self.sim.command("button keyboard 0")
        self.run()

    def stats(self):
        return self.sim.command("disk-state")["disk"]

    def until(self, predicate, budget=10000000):
        for _ in range(budget // 1000000):
            self.run(1000000)
            if predicate(self.state, self.stats()):
                return
        raise AssertionError(f"Diagnostic budget exhausted:\n{self.state['screen']}")


def monitor_errors(screen):
    for name in ("BUFFER", "SECTOR", "PARITY", "SEEK"):
        match = re.search(rf"\b{name} +(\d+) +(\d+)\b", screen)
        require(match is not None, f"Missing {name} error counters:\n{screen}")
        require(match.groups() == ("0", "0"), f"{name} errors: {match.groups()}\n{screen}")


def run_diagnostic(name, output=None, full_surface=True):
    require(name in TAPES, f"Unknown diagnostic {name}")
    model = 9370 if name.startswith("MD370") else 9374
    checks = []
    printer = b""
    with tempfile.TemporaryDirectory() as work:
        with Harness() as sim:
            sim.command(f"disk-model {model}")
            # A 9374 physical drive contains logical units 0 and 1. Both must
            # be online/protected for the surface verifier's preflight check.
            for drive in range(1 if model == 9370 else 2):
                sim.command(f"disk {drive} 0 {work}/{drive}.dsk")
                if name == "MA374_V1.2.B" or (name == "SV374_V1.1" and not full_surface):
                    sim.command(f"disk-protect {drive} 1")
            sim.command(f"printer {work}/printer.txt")
            sim.load(ROOT / f"tapes/diagnostics/{name}.tap")
            session = Session(sim)
            session.run(20000000)

            if name in ("MD370_V1.1", "MD374_V1.1"):
                require("MONITOR OBSERVES" in session.state["screen"], "Missing monitor help")
                session.keyboard_button()
                session.keys("5\n")
                session.until(lambda state, _: re.search(r"PASS\s+[1-9]\d*", state["screen"]))
                monitor_errors(session.state["screen"])
                checks.append("controller buffer test completed a pass with zero errors")
                session.keyboard_button()
                # Manual write/read functions exercise the selected sector.
                before = session.stats()
                session.keys("2\n")
                session.until(lambda _, stats: stats["writes"] > before["writes"])
                session.keyboard_button()
                session.keys("1\n")
                session.until(lambda _, stats: stats["reads"] > before["reads"])
                session.keyboard_button()
                monitor_errors(session.state["screen"])
                checks.append("manual disk write and read completed with zero errors")

            elif name == "MD370_V1.2":
                require("PHYSICAL DRIVE NUMBER" in session.state["screen"], "Missing drive prompt")
                session.keys("02F")  # Drive 0, sequential seeks, forward.
                session.until(lambda _, stats: stats["seeks"] > 410 and stats["max_cylinder"] == 202)
                require("SEEK ERROR" not in session.state["screen"], "Seek diagnostic reported an error")
                session.keyboard_button()
                require("SELECT TEST" in session.state["screen"], "KEYBOARD did not return to seek menu")
                checks.append("sequential seeks covered cylinders 0..202 and returned to menu")

            elif name == "MA374_V1.2.B":
                require("WHICH PHYSICAL DRIVE" in session.state["screen"], "Missing drive prompt")
                session.keys("0\n0\n0\n3\n")  # Physical drive, pack, head, sequential seek.
                session.until(lambda state, stats: "DESCENDING SEQUENTIAL SEEKS" in state["screen"]
                              and stats["seeks"] > 408 and stats["max_cylinder"] == 203)
                checks.append("sequential seeks traversed the 9374 range and changed direction")

            else:
                require("SURFACE VERIFICATION" in session.state["screen"], "Missing verifier prompt")
                session.keys("00PD")  # Physical drive, logical unit, physical/decimal errors.
                if full_surface:
                    session.keys("FY")  # Full verification; confirm destruction of scratch pack.
                else:
                    session.keys("RN")  # Read-only, no mechanical track offset.
                require("ENTER PACK SERIAL NUMBER" in session.state["screen"], "Missing serial prompt")
                session.keys("TEST\n")
                session.until(lambda state, _: "PASS #   1" in state["screen"], budget=60000000)
                require(re.search(r"ERRORS THIS PASS\s*=\s*HARD\s*:\s*0\s+SOFT\s*:\s*0",
                                  session.state["screen"]), "Surface pass reported errors")
                stats = session.stats()
                require(stats["max_cylinder"] == 203 and stats["max_head"] == 7
                        and stats["max_sector"] == 23, "Surface test did not cover the whole logical disk")
                require(stats["reads"] >= 39168, "Surface pass did not read every sector")
                if full_surface:
                    require(stats["writes"] >= 78336 and stats["formats"] >= 816,
                            "Surface pass omitted format or one of the write patterns")
                else:
                    require(stats["writes"] == stats["formats"] == 0, "Read-only test changed media")
                checks.append("full format/write/verify pass with zero hard/soft errors" if full_surface
                              else "whole-disk read-only pass with zero hard/soft errors")

            stats = session.stats()
            require(stats["errors"] == 0, f"Controller transfer failures: {stats}")
            result = {"tape": name + ".tap", "checks": checks, "disk": stats,
                      "instructions": session.state["instructions"], "screen": session.state["screen"],
                      "history": session.history}
        # Process exit flushes the diagnostic's local-printer output.
        printer = Path(work, "printer.txt").read_bytes()
    if output:
        output = Path(output)
        output.mkdir(parents=True, exist_ok=True)
        (output / f"{name}.json").write_text(json.dumps(result, indent=2) + "\n")
        (output / f"{name}.screen.txt").write_text(result["screen"])
        (output / f"{name}.printer.txt").write_bytes(printer)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(ROOT / "dp2200sim/diagnostic-results"))
    parser.add_argument("--read-only", action="store_true", help="run SV374 read-only instead of full verification")
    parser.add_argument("tapes", nargs="*", metavar="TAPE", help="diagnostic names without .tap; defaults to all five")
    args = parser.parse_args()
    results = []
    for tape in args.tapes or TAPES:
        result = run_diagnostic(tape, args.output, not args.read_only)
        print(tape + ": " + "; ".join(result["checks"]), flush=True)
        results.append({k: result[k] for k in ("tape", "checks", "disk", "instructions")})
    Path(args.output, "summary.json").write_text(json.dumps(results, indent=2) + "\n")
