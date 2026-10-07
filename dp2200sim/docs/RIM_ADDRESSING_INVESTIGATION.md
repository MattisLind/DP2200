# RIMTEST I/O addressing failure

Investigated 2026-10-07 against the unmodified `tapes/diagnostics/RIMTEST.tap`
(SHA-256 `f38ef550bc0520fce5a4e0f7d5cb27c479375acb186bab2cecf12fd1bbd2e151`).
All instruction addresses, bus addresses and masks below are **octal**.
Headless command arguments remain decimal.
The initial traces below describe exact-address dispatch and the intermediate
bit-0 experiment. The final section records the implemented mask decoder.
The independent disassembly review at the end explains how the documented
reserved addresses coexist, and why the unconditional bit-0 implementation
must not be treated as a faithful hardware decoder.

## What "left" and "right" mean

These labels come from the diagnostic itself. Its setup screens ask:

```text
I.D. for RIM under test (Left):
I.D. for RIM under test (Right):
I/O Address for RIM (Left):
I/O Address for RIM (Right):
```

The main display has separate left/right status, phase and pass columns.
Each side identifies a separate RIM module, with its own bus address and
network node ID. Our selected fixture is:

| Diagnostic side | Module | I/O address (octal) | Network node ID |
| --- | --- | --- | --- |
| Left | First RIM | `0234` | 1 |
| Right | Second RIM | `0232` | 2 |

The IDs and addresses are entered by the operator; "left" and "right" are
test/display labels, not fixed hardware port names. An I/O address selects a
module on the processor bus. A node ID identifies that module on the coaxial
network. Transmitter and receiver belong to the same module and use the same
I/O selection; their separate buffer pages are selected with COM1 commands.

The [9483 product specification](https://bitsavers.org/pdf/datapoint/arcnet/60911_9483_Resource_Interface_Module_Product_Specifications_19810720.pdf)
provides direct supporting evidence: section 7.2 assigns factory address
`0234` and reserves `0232`, `0231`, `0254`, `0252`, and `0251` for additional
RIMs attached to the same processor. Section 7.3 describes each module's
independently selected, unique network ID. Section 6.1 describes the parallel
processor bus daisy chain; section 2.2 permits two RIMs to connect directly
with one coaxial cable. This supports a test fixture with two physical modules
on one processor bus, connected to each other over coax.

The decoder probes below are a separate issue. Bus value `0235` is expected
to select the same RIM nominally configured at `0234`; it does not designate
a second module, a separate receiver address, or another network node ID.
The alias behavior is inferred from the diagnostic's bit tests and the bus
decoder architecture. The product specification documents one configured I/O
address per module and does not explicitly enumerate these decoder aliases.

The [February 1977 FRIL specification](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/arcnet/FRIL_Adapter_Feb77.pdf),
section 7.2, independently gives the same factory address and reservations
for additional modules. It does not supply a decoder schematic or explain
the alias tests. See the [FRIL/RIM comparison](FRIL_RIM_COMPARISON.md) for
the matching interface, revision differences and additional protocol detail.

## Finding

RIMTEST tests the hardware's address decoder, not just whether a controller
responds at its nominal address. It flips each of the eight address bits and
checks DA (Device Available, status bit 5, mask `040`). Changing a zero bit to
one must leave the RIM selected; changing a required one bit to zero must
deselect it.

With a RIM attached at `0234`, the first address issued by this check is
`0235`. The emulator supports only exact address lookup, so INPUT returns zero
at `0235`. RIMTEST interprets the missing DA bit as an address-decoder failure.
The failure occurs before buffer transfers, token handling or network traffic.
No fault in the numeric parser or XOR/AND/conditional-jump execution is needed
to explain this result.

## Exact executed path

After entering IDs `1`, `2`, and left address `234`, the numeric parser supplies
`E=234`. The caller saves it at memory `055236`, loads `C=234` and `D=040`, then
calls the address-check routine:

```text
026235  LCM                C = chosen address, 234
026236  LD 040             D = DA mask
026240  CALL 026357

026357  LB 001             B = bit mask, initially bit 0
026361  LAC                A = 234
026362  XRB                A = C XOR B = 235
026363  EX_ADR             select bus address 235
026364  EI
026365  INPUT              actual A = 000 (no exact-address device)
026366  NDD                A &= 040; Z = 1 because DA is absent
026367  LAC                A = 234; load preserves Z
026370  JTZ 026402         take the DA-absent branch
026402  NDB                A &= B; 234 AND 001 = 000, Z = 1
026403  JTZ 026416         zero bit was added, but no response: fail
026416  XRA                A = 0, Z = 1 (failure return)
026417  RET

026243  JFZ 026261         not taken: failure returned with Z = 1
026246  EX_BEEP
026247  LL 376
026251  LH 036             HL = 036376, the failure message
026253  CALL 035746         print "** I/O Addressing Failure **"
026256  JMP 026203         retry address entry
```

This is an ordinary INPUT, not PIN; no input-parity trap is involved. The
disassembler spells the address instruction `EX_ADR` with an underscore.
Searching traces for `EX ADR` would incorrectly suggest it never executed.

## Complete routine

The other branch and loop explain why responding only at `0234` is insufficient:

```text
026357  LB 001
026361  LAC
026362  XRB
026363  EX_ADR
026364  EI
026365  INPUT
026366  NDD
026367  LAC
026370  JTZ 026402          DA absent?
026373  NDB                DA present: test original address bit
026374  JFZ 026416          removing a one must not leave DA present
026377  JMP 026406
026402  NDB                DA absent: test original address bit
026403  JTZ 026416          adding a one must not remove DA
026406  SLCB               rotate mask B left, wrapping through bit 7
026410  JFC 026361          repeat until bit 7 rotates out
026413  ORBB               set flags from B=001; Z=0 means success
026415  RET
026416  XRA                Z=1 means failure
026417  RET
```

Its behavior can be expressed as:

```python
for bit in range(8):
    mask = 1 << bit
    select(chosen_address ^ mask)
    da_present = bool(input_status() & 0o40)
    expected_present = not bool(chosen_address & mask)
    if da_present != expected_present:
        return FAILURE
return SUCCESS
```

For left address `0234` (`0x9C`, binary `10011100`), the expected probes are:

| Bit | Mask | Probed address | Expected DA | Reason |
| --- | --- | --- | --- | --- |
| 0 | `001` | `235` | 1 | Add an unused line |
| 1 | `002` | `236` | 1 | Add an unused line |
| 2 | `004` | `230` | 0 | Remove a required line |
| 3 | `010` | `224` | 0 | Remove a required line |
| 4 | `020` | `214` | 0 | Remove a required line |
| 5 | `040` | `274` | 1 | Add an unused line |
| 6 | `100` | `334` | 1 | Add an unused line |
| 7 | `200` | `034` | 0 | Remove a required line |

## Hardware interpretation

The [5500 hardware reference, section 6.2.4, printed pp. 31–33](https://ftpmirror.your.org/pub/misc/bitsavers/pdf/datapoint/5500/60181-1_5501_5502_HardwareRef_1974.pdf)
describes address detection using four inputs and address straps; Figure 6.3
shows the decoder, parity gate and selection latch. Outputs are active low.
A strapped decoder observes its required lines rather than comparing all eight
bits. That architecture supports the behavior directly observed in RIMTEST.
The earlier manual describes 16 addresses using complementary nibbles; the
later RIM's documented factory address `0234` is not in that original subset.
Do not impose that older nibble restriction on RIM addresses.

For the RIM, the diagnostic establishes the selection predicate:

```cpp
(busAddress & strappedAddress) == strappedAddress
```

Thus `0234` requires A2, A3, A4 and A7. It also responds at `0235`, `0236`,
`0274`, `0334`, and combinations of additional asserted lines. The four unused
bits imply 16 selecting bus values, including the nominal address. The
[9483 specification, section 7.2](https://bitsavers.org/pdf/datapoint/arcnet/60911_9483_Resource_Interface_Module_Product_Specifications_19810720.pdf)
gives the factory and alternative RIM addresses. The predicate above is
derived from the diagnostic and the bus decoder architecture; that
specification does not itself explicitly spell out the malformed-address tests.

## Controlled verification

[probe_rim_addressing.py](../tests/probe_rim_addressing.py) reproduces both runs
using the real 5500 ROM initialization and the original tape. Captured routine
bytes, register traces, probe sequences and screens are retained in
[rim-addressing-probes.json](rim-addressing-probes.json).

```sh
make -C dp2200sim dp2200sim-headless
python3 dp2200sim/tests/probe_rim_addressing.py \
  --output /tmp/rim-addressing-current.json
```

1. Attach only the intended RIMs at decimal 156/154 (`0234`/`0232`), node IDs
   1/2. Left address validation probes only `235`, then prints the failure.
2. In a fresh process, also attach temporary independent RIMs at `233`, `235`,
   `236`, `272`, `274`, `332`, and `334`, with distinct node IDs. These supply
   DA at the expected zero-to-one aliases. The diagnostic executes all eight
   left probes and advances to the right address prompt.
3. Enter right address `232`. All eight right probes also complete:
   `233,230,236,222,212,272,332,032`. Execution reaches the main diagnostic
   display, showing both IDs/addresses and transmit/receive activity, without
   an addressing failure or `RAM DCL REQUIRED` message in this bounded run.

No emulator source or guest instructions were patched. This experiment isolates
the missing address responses as sufficient to explain the setup failure. The
temporary devices have separate buffers; they are experimental responders,
not correct aliases of the intended two devices. Reaching the main display
does not establish a completed pass. Stage 1 still cannot transmit or receive
packets over a link.

## Required correction and regression coverage

The controller currently stores the raw bus byte in `ioAddress` and uses
`isDeviceSupported(ioAddress)` plus `dev[ioAddress]` for dispatch. This rejects
the very first address RIMTEST intentionally probes. The defect is selection
in the I/O controller, not the RIM's DA flag: nominal-address status reads
already return DA correctly.

A correction should model RIM address straps, keep attachment addresses
separate from the current bus value, and route subsequent strobes to the
selected device's existing buffer/page state. Registering independent devices
at aliases or setting DA globally would conceal the defect without implementing
that behavior. Check the physical controller's valid strap combinations before
restricting the currently accepted arbitrary byte addresses.

Multiple controllers can select simultaneously for a noncanonical bus value:
`0236` matches both `0234` and `0232`. Selection must account for both rather
than arbitrarily choosing one. Both advertise DA during this probe, but the
manual describes tri-state input drivers, so arbitrary conflicting input bytes
must not be assumed to combine by a wired-OR rule without further evidence.
Output strobes and STATUS/DATA selection should reach every selected responder.

The eventual regression should require all eight original left and right
checks to succeed with just the two intended devices, verify all 256 bus
values against the selection predicate for an isolated RIM, and show that
DATA/WRITE through aliases access the same buffer and pointer as the nominal
address. Also retain absent-device, deselection and multi-selection checks.
The initial investigation retained exact-address dispatch. Its archived
captures describe the original failure; subsequent changes are recorded below.

## Follow-up: two-address model and a single attached RIM

The requested minimal update now makes `0234` and `0235` access one shared
device. The alias does not allocate another buffer or node. Both addresses are
reserved at attachment; overlapping modules are rejected. Only bit 0 is ignored
by this model, which is a partial implementation of the decoder described above.

Fresh runs of the original diagnostic with only `rim 156 1` attached show:

1. Enter IDs 1/2 and left address `234`. Probe `235` returns status `261`,
   including DA. The routine follows its success branch and rotates B to `002`.
2. It then selects `236` at `026363`. INPUT returns `000`, and the same
   DA/original-bit comparison returns failure. The message is still printed
   during left address setup, now on its **second** probe.

The left-only model therefore lets us investigate the decoder without needing
to configure a right module. It does not complete the address test or reach
packet tests. The requested bit-0 alias and the diagnostic's complete four-line
decoder expectation remain distinct requirements.

To investigate optional right-side setup beyond that left-side blocker, separate
control runs add responders at `236`, `274`, and `334` to satisfy the remaining
left probes. No module is attached at `232` or `233` in those runs. These extra
devices are experimental DA responders, not a successful one-module fixture.

| Input | Observed diagnostic behavior |
| --- | --- |
| Blank right ID (Enter) | Advances to left address entry; it does not remove the right-side setup |
| Right ID `0` | Repeats the right ID prompt |
| Right ID `*` | Returns to left ID entry |
| Blank right address (Enter) | Reuses the previously parsed address `234`; duplicate-address check beeps and repeats the right prompt |
| Right address `0` | Runs the address check and prints addressing failure |
| Right address `*` | Returns to left address entry |
| Right address `232`, right device absent | Probes `233` and prints addressing failure |
| Right address `235`, the left module's alias | Probes `234`; DA remains set when bit 0 is cleared, so the right-side decoder check fails |

The blank-address trace loads `C=234` at `026322`, loads the configured left
address at `026327`, compares them at `026330`, and takes `JTZ 026353` to beep
and retry. This explicitly rejects using the same nominal I/O address for both
test sides. The normal right-address path invokes the same DA decoder check as
the left side. These inspected paths provide no blank/zero skip option. An
unmodified complete RIMTEST run still needs two available modules at distinct
nominal addresses.
Using `235` as the right-side address therefore cannot turn one module into
the two modules the diagnostic expects: its first bit test rejects that alias
as a nominal strapped address.

The updated [probe script](../tests/probe_rim_addressing.py) captures all these
experiments. Their traces and screens are in
[rim-bit0-alias-probes.json](rim-bit0-alias-probes.json); the earlier
`rim-addressing-probes.json` is retained as historical evidence.

```sh
python3 dp2200sim/tests/probe_rim_addressing.py \
  --output /tmp/rim-bit0-alias-probes.json
```

## Independent review: reserved addresses and apparent interference

The reserved second and third addresses expose a flaw in describing the device
as having an unconditional even/odd address pair. Which address lines are
required depends on the configured address: bit 0 is unused at `0234` and
`0232`, but **required at `0231`**. The intermediate experimental bit-0 code would
incorrectly select the `0231` module at `0230` too. It has now been replaced with
address-specific selection. `0230` is not one of the documented reserved
addresses; the overlap relevant to the second/third modules is the probe
value `0233`.

The [independent disassembly](RIMTEST_ADDRESS_CHECK.lst) was generated from
the original tape, rather than the simulator's disassembler or instruction
trace. The 33 routine bytes occur uniquely at tape file offset 5326 decimal.
Little-endian jump operands agree with both branches previously traced:

```text
026366  243         NDD             A = input status AND D; D=040
026367  302         LAC             A = chosen address; flags unchanged
026370  150 002 055 JTZ 026402       if DA absent, take absent branch
026373  241         NDB             DA present: A = chosen address AND B
026374  110 016 055 JFZ 026416       FAIL if the original bit was one
026377  104 006 055 JMP 026406       otherwise advance the mask
026402  241         NDB             DA absent: A = chosen address AND B
026403  150 016 055 JTZ 026416       FAIL if the original bit was zero
```

The [5500 hardware reference](https://ftpmirror.your.org/pub/misc/bitsavers/pdf/datapoint/5500/60181-1_5501_5502_HardwareRef_1974.pdf)
confirms register loads preserve flags (section 5.8.3), ND/XR update zero,
JT/JF test the indicated flag, and the `111` prefix selects B for SLC/OR
(section 5.8.4). Thus the intervening LAC does **not** overwrite the DA result
used by JTZ. The independent decode confirms this really is a selection test
with a zero-to-one probe expected to respond. It does not reveal a reversed
branch, inverted DA bit, or hidden test of separate transmit/receive addresses.

All six documented nominal addresses have exactly four set bits:

| Nominal address (octal) | Binary A7..A0 | Bits set |
| --- | --- | --- |
| `234` | `10011100` | 4 |
| `232` | `10011010` | 4 |
| `231` | `10011001` | 4 |
| `254` | `10101100` | 4 |
| `252` | `10101010` | 4 |
| `251` | `10101001` | 4 |

Under the inferred four-input decoder `(bus & configured) == configured`,
one distinct four-bit nominal address cannot select another: containing all
four of another address's required bits, while having only four bits itself,
would require the two addresses to be identical. There is therefore no
second/third-module interference at the documented **nominal** addresses.

Adding a bit produces a five-bit noncanonical probe value. That can satisfy
two decoders:

| Value presented on bus | Modules selected among the six reserved addresses |
| --- | --- |
| `234` | `234` only |
| `232` | `232` only |
| `231` | `231` only |
| `233` | `232` and `231` |
| `235` | `234` and `231` |
| `236` | `234` and `232` |

For example, `232 AND 231 = 230`, so neither nominal address contains the
other's required bits. But `233 AND 232 = 232` and `233 AND 231 = 231`, so
the probe `233` selects both. Removing a required bit instead creates a
three-bit value, which cannot select any of these four-input decoders.

RIMTEST deliberately generates these noncanonical values. It reads with
ordinary INPUT and masks to DA, which all responding modules set. Its check
does not interpret their other status bits or verify the bus parity during
these probes. This software behavior is compatible with multiple responders
agreeing on DA. How conflicting bits behave electrically still needs circuit
evidence; these results do not establish a general input-byte combining rule.

[disassemble_rim_address_check.py](../tests/disassemble_rim_address_check.py)
also contains a bounded independent evaluator of the 33 raw routine bytes.
It implements the documented register, zero/carry, branch and rotate
operations and accepts a supplied DA-response predicate. With all six modules
modeled by the mask rule, all **48** single-bit checks succeed and every
nominal address selects only its own module. Exact lookup and an unconditional
bit-0 pair both fail the routine. This evaluator validates the interpretation
of the code; it is not evidence of a working emulator decoder or electrical bus.

The eight single-bit outcomes are directly established by the diagnostic.
Responses to combinations of multiple additional bits follow from the
four-input decoder interpretation; RIMTEST's routine does not itself exhaust
all 256 bus values.

```sh
python3 dp2200sim/tests/disassemble_rim_address_check.py
```

## Implemented mask decoder

The current controller selects RIMs using `(bus & configured_mask) ==
configured_mask`. Attachment accepts an unoccupied byte mask containing exactly
four one bits, plus a unique nonzero node ID. All six documented nominal
addresses are supported simultaneously. Other emulated peripherals retain
their existing exact-address dispatch.

EX ADR collects every matching RIM and enters STATUS mode on each. Subsequent
WRITE, COM1–4, STATUS and DATA operations reach the entire selected set, and
each input operation reads every selected responder once. All aliases use the
module's own buffer/page/pointer state. No independent alias devices are created.
Newly attached modules wait for an EX ADR strobe before responding.

The electrical result of contradictory input drivers remains unspecified by the
documents inspected. The implemented emulator policy is explicit: common one
bits are retained, disagreeing bits become zero, and PIN signals input parity
failure if responders disagree. Thus agreeing DA status bits survive the
diagnostic probes, and conflicting bytes are not silently read from an arbitrary
first module. The policy does not claim to reproduce physical driver voltages.
Host `rim-state`/`rim-event` requests require exactly one matching module and
reject ambiguous aliases instead of silently inspecting/resetting one of them.

Measured against the unmodified tape:

- Actual `0234`/`0232` modules, IDs 1/2: both complete eight-probe sequences and
  enter the main test display without an addressing or RAM DCL error.
- All six nominal modules attached: the left/right checks likewise complete,
  including probe addresses selecting multiple RIMs.
- Only `0234` attached: all eight left probes pass; right address `0232` fails
  on its first probe `0233` because the right module is absent.

Fresh captures are in [rim-mask-decoder-probes.json](rim-mask-decoder-probes.json).
The probe script now runs the implemented decoder with real modules; earlier
JSON files retain the exact-address and bit-0 experiments as historical evidence.
The native regression suite checks all 256 bus values for each reserved mask
(1,536 cases), shared-buffer aliases, required-bit deselection, six-module
nominal isolation, multi-responder strobes/reads, agreeing PIN, conflicting
INPUT/PIN, block transfers with wrap and selection-latch behavior. Both builds
and all 58 headless tests (18 RIM checks) pass.

This completes the address-decoder prerequisite. Modules still have no virtual
coax/token transport; main-display entry is not a completed RIMTEST packet pass.

```sh
make -C dp2200sim all test-headless
python3 dp2200sim/tests/probe_rim_addressing.py \
  --output /tmp/rim-mask-decoder-probes.json
```
