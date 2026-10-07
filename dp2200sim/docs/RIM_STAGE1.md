# 9483 RIM Stage 1

Implemented 2026-10-07: optional 5500 RIM register/buffer devices and tested
CPU block transfers. Networking, token processing and a passing RIMTEST remain
later stages. See [RIM_FEASIBILITY.md](RIM_FEASIBILITY.md) for the overall plan.

## Enable and inspect

Build with `make -C dp2200sim all dp2200sim-headless`.
In the interactive command window, before loading the program:

```text
SET CPU=5500
RIM ADDRESS=156 NODE=1
RIM ADDRESS=154 NODE=2
```

These command parameters are **decimal**: 156/154 correspond to octal
`0234`/`0232`. Each command attaches a separate module with one configured
bus address and its own network node ID. RIMTEST calls these modules "left"
and "right" and prompts for an ID and I/O address for each; these labels
refer to its two test/display sides. `RIM` alone uses address 156 and node 1.
Attachment requires the 5500, a free byte-sized address mask with exactly four
set bits, and a unique nonzero byte-sized node ID.
Devices are absent unless explicitly attached. Occupied addresses and duplicate
IDs are rejected rather than replacing another device.

Address selection now uses `(bus_address & configured_mask) == configured_mask`.
The four required one bits select the module; the other four bits are ignored.
All 16 selecting bus values access the **same module**: buffers, byte pointer,
page registers, STATUS/DATA mode and flags. Clearing any required bit deselects
it. This replaces the earlier experimental bit-0 pairing. Nominal addresses
`0234`, `0232`, `0231`, `0254`, `0252`, and `0251` can all coexist. See the
[independent disassembly review](RIM_ADDRESSING_INVESTIGATION.md#independent-review-reserved-addresses-and-apparent-interference)
for the address-specific decoder interpretation and coexistence of all six
reserved addresses.

Noncanonical bus values may select several modules: `0233` selects `0232` and
`0231`; `0236` selects `0234` and `0232`. WRITE, COM1–4, STATUS and DATA strobes
reach every selected module. Reads consume each selected buffer once. When
input bytes disagree, the emulator retains common one bits and resolves
disagreeing bits to zero; PIN dispatches an input parity fault. This is a
deterministic contention policy, not a claim about the physical tri-state bus.
The shared DA bit remains set during status probes.

For a single-module fixture, issue only `RIM ADDRESS=156 NODE=1` (or headless
`rim 156 1`). No right module is automatically created.

The headless equivalents are:

```text
cpu 5500
rim 156 1
rim 154 2
rim-state 156
```

`rim-state ADDRESS` is observational: node, status, byte pointer, all three page
registers, DATA/STATUS mode and pending disable commands. It neither advances
the CPU nor consumes a buffer byte. `rim-event ADDRESS reset|recon` supplies
explicit reset/reconfiguration events for testing; it does not create a link.
Both commands accept an alias only when it identifies exactly one attached
module. Ambiguous aliases are rejected. Modules attach unselected and start
responding after an EX ADR strobe.

## Behavior and boundaries

The implemented commands and status mapping target the
[9483 specification, sections 3.1–3.2](https://bitsavers.org/pdf/datapoint/arcnet/60911_9483_Resource_Interface_Module_Product_Specifications_19810720.pdf).
Ordinary and block I/O use the same device buffer path. STATUS reads preserve
the byte pointer, and WRITE still writes buffer memory when STATUS mode is
selected. Reselecting the device enters STATUS mode. The three page registers
remain independent.

Fresh buffers and page/pointer registers start at zero as an emulator policy.
Explicit RIM reset retains buffer/page/pointer contents, enters STATUS mode,
sets ready/reset/device flags and clears pending disables. Indeterminate
hardware buffer/parity contents are not modeled as random values. Reserved
command encodings are ignored; unsupported COM2/COM3 strobes do nothing.

Transmit/receive enable commands record their pages and clear the respective
ready flag. Re-enabling a busy engine is ignored. A disable request on a busy
engine is retained for a future token event; this isolated Stage 1 device has
no link that could complete it. No packet is received and TMA is never invented.
POR and injected RECON can be cleared through guest commands; IPE clearing is
decoded, but parity fault generation/injection is not implemented yet.

The active CPU decoder already supported MIN/MOUT. Stage 1 adds measured
instruction costs of 8.4/8.8 microseconds per byte, respectively, using the
[5500 hardware reference, printed pp. 19–20](https://ftpmirror.your.org/pub/misc/bitsavers/pdf/datapoint/5500/60181-1_5501_5502_HardwareRef_1974.pdf).
Transfers stop on CPU memory/device errors instead of continuing silently.
HL and C retain progress from completed bytes on an early return. User-mode
execution takes the existing privileged-instruction fault path without I/O.
Timing is charged at the instruction boundary; individual intra-instruction
bus strobes are not scheduled as separate callback events.

## Regression checks

```sh
make -C dp2200sim test-rim
make -C dp2200sim test-headless
```

[test_rim.py](../tests/test_rim.py) runs native machine programs against the
actual CPU/controller implementations, covering:

- All four pages; pointer wrap from 255 to zero; STATUS, DATA, PIN and WRITE.
- Device addressing and isolation between two attached RIMs.
- All 256 bus values against each of the six reserved masks (1,536 cases),
  including buffer writes through every selecting alias and deselection.
- Six simultaneous modules with distinct buffers at their nominal addresses.
- Multiple responders receive all commands; agreeing reads, ordinary conflicting
  reads, PIN conflict/fault dispatch, and shared MIN/MOUT with pointer wrap.
- Four-one-bit mask validation, occupied addresses and duplicate node rejection.
- Unambiguous host inspection/events and attachment selection-latch behavior.
- An unselected bus remains absent even when address 255 is a RIM alias.
- Original RIMTEST completes both eight-bit address checks with `0234`/`0232`
  simultaneously attached and reaches the main diagnostic display.
- Independent page registers, busy-engine selection and pending disables.
- Initial flags, clear POR/RECON, explicit reset and reserved command handling.
- MIN/MOUT C values 0, 1, 15, 16, 17, 31, 128 and 255: exact byte counts,
  resulting registers/flags, pointer wrap and per-instruction simulated time.
- Scalar readback of MOUT data against independent source patterns, including
  unchanged bytes beyond the requested transfer.
- A complete 256-byte MOUT/MIN loop, with independent readback comparison.
- User-mode block I/O leaves device pointers, registers and memory untouched.

Validation: interactive and headless builds succeeded. Mask-decoder regressions
are included in the headless suite; the original setup is checked using the
unmodified diagnostic tape, with real modules rather than stand-in responders.
All 58 headless tests, including 18 RIM checks, passed.

These are register/buffer regressions, not a substitute for the real diagnostic.
With the real `0234`/`0232` fixture, RIMTEST now passes both address checks and
reaches its main display without an addressing or RAM DCL error in the bounded
run. Both checks also pass with all six reserved modules attached. A single
`0234` module passes the left check but cannot supply the diagnostic's right
module. Captures are in [rim-mask-decoder-probes.json](rim-mask-decoder-probes.json);
the [addressing investigation](RIM_ADDRESSING_INVESTIGATION.md) retains the
earlier failures, independent disassembly and current implementation details.
Networking/token completion remains Stage 2: no completed packet-test pass,
general parity-injection coverage or 6600 RIM support is claimed here.
