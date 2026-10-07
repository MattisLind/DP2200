# Diagnostic tape inventory and peripheral feasibility

Investigation: 2026-10-06. Scope: all **41 `.tap` files** in
`tapes/diagnostics/`, including utilities and data mixed into that directory.
The filenames alone do not reliably describe everything on a tape.

Subsequent [cassette verification](CASSETTE_DIAGNOSTICS.md) extends the original
observations below: EXRCASS transport and bounded TAPTIM data checks now pass
on both decks after functional fixes; TAPTIM timing tolerances still fail.
The linked boot/exercise JSON files remain the original inventory observations.

Every image was structurally inspected and booted in a fresh headless simulator
with each of the 2200, 5500 and 6600 CPU modes: **123 boot probes**. Selected
programs were then exercised interactively. Existing CPU, cassette and disk
regressions were also run: **30 tests passed**.

The best next peripheral project is a **9460/9462 multiport asynchronous
communications interface**, followed by a serial terminal and printer backend.
It would unlock several independent diagnostics and DATASHARE software. The
9370/9374 disk diagnostics already exercise implemented hardware. The
synchronous communications interface, reel tape controller and ARC resource
interface are feasible separate projects, with increasing protocol complexity.

## Evidence and reproduction

- [Boot results](diagnostics-boot-results.json): image SHA-256, byte and record
  counts, file labels, framing/checksum findings, and three CPU runs per tape.
- [Interactive results](diagnostics-exercise-results.json): command sequences,
  host keys, screen snapshots, halts and selected disk statistics.
- [Inventory runner](../tests/inventory_diagnostics.py) and
  [interactive experiment runner](../tests/exercise_diagnostics.py).
- [Existing diagnostic coverage](README.md#9370-and-9374-disk-diagnostics) and
  [CPU/ROM coverage](README.md#test5556-and-the-5500-rom).

From the repository root:

```sh
make -C dp2200sim dp2200sim-headless
python3 dp2200sim/tests/inventory_diagnostics.py \
    --output /tmp/diagnostics-boot-results.json
python3 dp2200sim/tests/exercise_diagnostics.py \
    --output /tmp/diagnostics-exercise-results.json
make -C dp2200sim test-headless
```

Each boot probe has a five-million-instruction budget, with snapshots at
100,000, one million and five million instructions, stopping early on HALT.
5500/6600 runs initialize the supplied 5500 ROM at octal `170036` for up to one
million instructions before loading the cassette bootstrap at address zero.
No keys, disks or output devices are attached in the initial boot probes.
Thus a blank disk bootstrap is expected to wait; a working menu is evidence
of loading and console operation, not a completed hardware test. Running an
old tape in all three modes does not establish historical CPU compatibility.

Follow-up experiments use the CPU indicated below, longer bounded runs,
explicit keys/buttons, read-only source tapes, a write-protected temporary disk
for SURVAR, and the existing read-only DOS.C floppy fixture. Only disposable
files receive printer output or disk creation. The runners do not repair tapes,
change the CPU implementation or declare every run a pass. Mechanical alignment,
lamps, physical keyboard switches and printed page quality require separate
interpretation from software-visible behavior.

## Tape integrity and layout

All **6,704 framed records** could be traversed, with matching leading/trailing
little-endian lengths, valid recognized file headers, and **zero failed numeric
record checksums**. These images use the repository's unpadded length/payload/
length cassette framing; this is not a validator for every general SIMH tape
format. Bootstrap checksums and program-specific CRCs are not covered by the
numeric-record checksum check.

The conventional numeric program-record address/complement check flags records
in `DEMO` (44), `HRLYMEMT` (256), `PCMTEST` (5), `UBOOT` (15), and `tstpro1.1`
(37). Those counts are recorded rather than silently ignored. Several images
contain non-program data, release material or unrelated material after their
initial program/end label. An address-complement mismatch in that material is
not sufficient evidence of corruption in the runnable program. PCMTEST and
TSTPRO actually execute successfully despite these flags.

Useful exact relationships:

- `COPY06.tap` is exactly `COPY06_1.tap` with its first **532 bytes** removed:
  one 512-byte bootstrap record plus one file-0 header. It begins with a numeric
  record, so it is not independently bootable through `load`.
- `DOSC_BOOT.tap`, `UBOOT.tap` and `UBOOT-2.tap` have identical first framed
  records: **504 payload bytes, 512 framed bytes**. `UBOOT-2` consists entirely
  of that bootstrap. Its purpose is demonstrated by booting DOS.C from floppy.
- `DEMO.tap` begins with a four-byte file header, not a machine bootstrap.
- `DIAG6600_V1.1.tap` is the repaired image described in
  [the tape-repair notes](README.md#diag6600-tape-repair). No original backup
  image is retained in this directory.

## Inventory of every image

“Prompt” or “menu” below means the bounded run reached that screen. It does not
mean the requested peripheral test completed. Purpose comes from observed
screens and embedded text unless explicitly identified as an inference.
Addresses in this document are octal when prefixed with `0`.

| Tape | Identification and target | Observed result and coverage |
| --- | --- | --- |
| `5500PROCTEST_V2.1.tap` | 5500 processor test 2.1, 20/05/75. Console keyboard/display, 2200 instructions, 5500 memory/instructions and repeated cycle. | Initial blank wait at PC `000760`; pressing KEYBOARD reaches the six-option menu on 5500. Individual instruction/memory options not certified. |
| `BAUDRATE.tap` | BAUDRATE tester 3.4; baud/strapping aid for 9460/9462 interfaces and 3600 terminals. | Shows address entry and baud-rate strapping charts in all modes. Actual rate measurement needs communications hardware. |
| `COPY06.tap` | German COPY.06 cassette copier, incomplete bootable prefix. | Immediate blank HALT; exact suffix of `COPY06_1`, missing bootstrap and file-0 label. Cannot test copying through standalone `load`. |
| `COPY06_1.tap` | Bootstrapped German COPY.06 cassette copier; source front deck, blank destination rear deck. | Blank HALT at `010444` after 1,952,473 instructions in all modes. One RUN/continue retry did not establish a working copier menu. Cause unresolved. |
| `COPY_V1.9.tap` | Tape duplicating program 1.9; two internal cassette decks. | Working instructions screen in all modes, waiting for replacement tapes and KEYBOARD. A complete image duplication was not performed. |
| `CTOS_V3.2_GEDIT_V2.7_LISTER_V2.3_COPY_V1.9_ASM_V4.6.tap` | CTOS 3.2 cassette operating system. Filename identifies editor, lister, copier and assembler bundle; file labels 0–5 observed. | CTOS `READY` in all modes. Bundled tools were inventoried from labels/text/name, not individually executed. |
| `CTOS_V3.2_KE5302.tap` | CTOS 3.2 with KE5302 application, as identified by filename. File labels 0–2. | CTOS `READY` in all modes; KE5302 application workflow not exercised. |
| `DEMO.tap` | German GIER/Datapoint demonstration text/data describing data capture and 1200-bps transmission. | Begins with file-0 header; blank HALT after two instructions. Treat as a data/library tape, not a verified standalone program. |
| `DIAG2200_V1.1.tap` | 2200 diagnostic collection: PCMTST 1.1, TP1122 1.1, TAPTIM 2.1, KDTEST 1.2, HRLYMENT 1.2. | All five entries load on 2200. Memory error grid stays zero in bounded run; TP1122 reaches BETA/pass 1 with repeated PUSH/POP output, not a clean completed-suite claim. See collection details below. |
| `DIAG5500_V1.1.tap` | 5500 diagnostic collection: UNITEST 1.2, TEST5500 2.2, TEST5556 1.1, TAPTIM 2.1, KDTEST 1.2, MOVI 2.1. | Main menu works. Entries 1–5 reach their program prompt/menu. MOVI loads after KEYBOARD retry. TEST5556 option E completes in existing regression. See follow-up details below. |
| `DIAG6600_V1.1.tap` | 6600 diagnostic collection: UNITEST 1.2, TST6600C 1.1, TST6600D 1.1, TAPTIM 2.1, KDTEST 1.2, MOVI 2.1. | Repaired main menu works. UNITEST reaches printer-log prompt; Both TST6600C/D report `WRONG MICRO-CODE ROM` after retry. TAPTIM/KDTEST/MOVI load after KEYBOARD retry; details below. No full 6600 suite pass. |
| `DOSC_BOOT.tap` | Disk bootstrap with DOS.C-related utility material. Targets floppy controller/disk boot, not a peripheral diagnostic. | Blank wait without disks; reaches DOS.C 2.4 `READY` with `tapes/DOS.C/003.IMD` in floppy 0 on 2200. |
| `EM3360_V1.1.tap` | Datapoint 3360-102 terminal emulator 1.1. Communications application, not a 3360 hardware test. | Emulator banner and `BAUD RATE?` in all modes. No serial host attached. |
| `EM3780B_V3.1.tap` | IBM 3780 remote-job-entry emulator 3.1. Synchronous communications, cassette/console and optional printer, reel tape/card reader. | Full emulator command menu in all modes. Transmission/reception needs a 9404-compatible interface and protocol peer. |
| `EM3780TRACE_V3.1.tap` | 3780 emulator diagnostic trace 3.1; embedded output selector offers local/servo printer or CRT. | Blank HALT at `024615` after 1,093,642 instructions in all modes; a continue retry did not yield trace output. Catalog describes it as support for the emulator. It is not an independent peripheral pass. |
| `EXRCASS_V1.1.tap` | Internal cassette transport exerciser 1.1: select deck, slew forward/backward, rewind, stop and halt. | Menu works in all modes. Front/rear selection and STOP commands responded in follow-up. Full transport/speed/endurance not verified. Contains substantial trailing material. |
| `EXRIBM_V2.3.A.tap` | Seven-/nine-track industry-compatible reel tape exerciser 2.3.A; default controller address `0264`. | Track prompt in all modes; choosing 9 and accepting default reaches exerciser control screen. No reel tape controller is implemented, so data operations are untested. |
| `EXTDISPTEST.tap` | Display exerciser, J/W November 1977. Cursor, horizontal/vertical/sliding characters, erase, roll, input, print, bell/beep/click. Embedded port text suggests an external-port variant; default explicitly says CRT. | Local CRT menu in all modes. `Z` increases beep count to 2 and returns to menu. External terminal port mode not demonstrated. |
| `HRLYMEMT.tap` | Hourly memory test for Datapoint 2200 Version II; explicitly excludes Version I. | Banner reports 16K; longer 2200 run advances `MINUTES` to 10 without observed error output. Not a full hour or retention qualification. Trailing unrelated material is present. |
| `KE0105.tap` | German KE01.05 data-entry application. Prompts for tape and beginning/continuing data acquisition. Exact business use unknown. | Program menu in all modes. No completed data-entry/write workflow. |
| `KE1504.tap` | German KE15.04 data-entry application; additional cancellation/order-field text. Exact business use unknown. | Program menu in all modes. No completed data-entry/write workflow. |
| `LGO_A.tap` | Mixed image containing DOS LGOPROG 1.2 standalone-program loader and additional objects/data. | Blank initial HALT. Continuing on 5500 produces ROM-monitor-style octal output and beep, not a working LGOPROG session. Intended loading context remains unresolved. |
| `MA374_V1.2.B.tap` | 9374 20-MB disk alignment/seek diagnostic: head/sector alignment, sequential and alternate seeks. | Menu in all modes. Existing protected-media regression verifies sequential seeks traversing address range and reversing direction. Physical alignment is not emulated. |
| `MD370_V1.1.tap` | 9370 mass-storage disk diagnostic 1.1; controller buffer and manual disk operations. | Help screen in all modes. Existing regression completes buffer pass and manual sector write/read with zero monitor error counters. |
| `MD370_V1.2.tap` | 9370 maintenance aid 1.2; alignment and sequential seeks. | Drive-number prompt in all modes. Existing regression verifies forward cylinder seeks 0–202 and KEYBOARD return. |
| `MD374_V1.1.tap` | 9374 mass-storage disk diagnostic 1.1. | Help screen in all modes. Existing regression completes controller buffer pass and sector write/read with zero monitor counters. |
| `MPXTEST.tap` | Multiport adaptor diagnostic. Address jumper checks, eight-port transmit/receive loopback and modem-control signal tests. | Address prompt in all modes. Attempts `151` and `105` yield `ILLEGAL ADDRESS`; no interface exists at requested address. Hardware loopback not run. |
| `PCMTEST_V1.1.tap` | Pathological-case 2200 memory test 1.1, March 21, 1977; per-bit/per-1K error grid. Also contains release material. | Autonomous test runs in all modes. Longer 2200 run reaches pass 4, row 22; all displayed error counters zero. Strongest newly exercised memory diagnostic. |
| `PRINTERTEST.tap` | Serial/terminal printer test through 3360/3600, single/multiport adaptor or external modem. Tests character set, CR/LF/FF, right margin, print position and echo. | Connection-selection prompt in all modes. It is not just the existing local-printer output stream; missing serial/terminal path blocks complete tests. |
| `RIMTEST.tap` | ARC Resource Interface Module diagnostic 1.1; paired left/right RIMs, node IDs, I/O addresses, transfers and status/parity checks. | 2200 blank HALT; 5500/6600 reach left-node-ID prompt. Embedded `RAM DCL REQUIRED` and I/O-addressing failures identify additional prerequisites. No RIM implemented; no network pass. |
| `SURVAR_V1.1.tap` | 2316 disk-pack surface verification 1.1, consistent with 9370 removable-pack geometry. Full overwrite or read-only CRC/sector checking with local-printer report. | Prompt in all modes. New protected 9370 follow-up completes read-only verification: 97,440 reads, zero writes/formats/errors, zero bad sectors. Details below. |
| `SV374_V1.1.tap` | 9374 logical-disk surface verifier 1.1; format/write/read patterns or protected read-only mode. | Prompt in all modes. Existing regressions pass both full and read-only verification, with both logical units attached. |
| `TAPTIM_V1.5.tap` | Internal cassette timing/speed/endurance/inter-record-gap diagnostic 1.5. | Scratch-tape instructions/menu in all modes. Mechanical timing and endurance not certified by booting it. |
| `TST404.tap` | 9900-404 synchronous communications adaptor test, standard address `0245`; automated/manual receive/transmit and clock tests. Corresponds to 9404 interface family. | Address prompt in all modes; accepting default reaches test menu. Starting `A` reaches CTS-lamp confirmation and waits for DISPLAY. No adaptor present; no completed test pass. |
| `TSTDIS.tap` | Internal 2200 CRT diagnostic: character set, alternating full lines, roll, erase, cursor and lights. | Instructions screen in all modes. Three DISPLAY press/release cycles advance into character/roll/control displays. Physical display appearance still needs UI inspection. |
| `TSTKEY.tap` | Internal 2200 keyboard diagnostic: old/new keyboard, EO 2910, row/shift/keypad comparisons. | Keyboard-type prompt in all modes; `N` then ENTER reaches expected first-row/backspace sequence. Full physical key/shift coverage not performed. |
| `TSTUBE55_V1.2.tap` | DATASHARE system diagnostic 1.2, 30 June 1980; multiport interface, 8200/3601 terminals, 9408 answer modem and serial printers. | Initial blank wait; KEYBOARD on 5500 yields `THERE IS NO MULTIPORT 1 (ADDRESS 0151) ENTER ADDRESS:`. Clear missing-peripheral evidence. |
| `TSTUBE_V1.A.tap` | DATASTATION diagnostic 1.A; up to 16 ports of 3360/3600 terminals, cursor/display/roll/erase/keyboard/printer tests. | Port-configuration/menu screen in all modes. Terminal tests need a multiport adaptor and attached terminal model. |
| `UBOOT-2.tap` | Universal disk bootstrap only, one record; same bootstrap as DOSC_BOOT. | Waits blank without disks. Boots DOS.C 2.4 `READY` from floppy fixture on 2200. |
| `UBOOT.tap` | Same universal disk bootstrap followed by data including German sample parts/items records. | Waits blank without disks. Boots DOS.C 2.4 `READY` from floppy fixture on 2200. Appended data is not itself a diagnostic. |
| `tstpro1.1.tap` | 2200 instruction-test regression fixture; also contains trailing unrelated data. | `TEST COMPLETED` and HALT at `004052`, after 1,439 instructions in all modes. Existing regression also passes. |

## Results inside the diagnostic collections

The collection tapes matter because a successful first menu does not prove that
all their files can be loaded or executed. Every advertised selection was
attempted on its matching CPU, with 25 million instructions after selection.

| Collection / selection | Result |
| --- | --- |
| DIAG2200 / 1 PCMTST | Runs the pathological memory test; zero displayed bit/1K error counters during the bounded run. |
| DIAG2200 / 2 TP1122 | Reaches BETA mode, pass 1; screen fills with repeated `PUSH/POP 1`. This warrants investigation of stack/instruction semantics before calling the whole suite passed. |
| DIAG2200 / 3 TAPTIM | Loads 2.1 and asks `ALIGNMENT ONLY? (Y OR N)`. |
| DIAG2200 / 4 KDTEST | Loads 1.2 keyboard/display menu: alignment, characters, display and keyboard. |
| DIAG2200 / 5 HRLYMENT | Runs hourly-memory screen and advances minute counter. |
| DIAG5500 / 1 UNITEST | Loads universal processor memory test 1.2; waits at local-printer logging prompt. |
| DIAG5500 / 2 TEST5500 | Loads 2.2 processor test menu. Existing research documents a hardcoded ROM expectation different from supplied firmware; no new whole-suite pass claimed. |
| DIAG5500 / 3 TEST5556 | Loads 1.1 processor menu. Existing regression executes option E including final BCV and returns to menu. |
| DIAG5500 / 4 TAPTIM | Loads 2.1 alignment prompt. |
| DIAG5500 / 5 KDTEST | Loads 1.2 keyboard/display menu. |
| DIAG5500 / 6 MOVI | Initially waits in loader; KEYBOARD press/release reaches moving-inversion memory test 2.1 prompts for hold-on-error, printer logging and starting sector (0–11). Embedded text also names MOVIA/MOVIF/MOVI choices. No complete moving-inversion pass performed. |
| DIAG6600 / 1 UNITEST | Loads 1.2 and waits at local-printer logging prompt. |
| DIAG6600 / 2 TST6600C | Reports `WRONG MICRO-CODE ROM`; supplied 5500 firmware is not the expected 6600 microcode/ROM environment. |
| DIAG6600 / 3 TST6600D | Initially stays in loader; KEYBOARD retry reports `WRONG MICRO-CODE ROM`, like TST6600C. No 6600 instruction certification. |
| DIAG6600 / 4–6 | Initially wait in loader. KEYBOARD press/release loads TAPTIM alignment prompt, KDTEST menu and MOVI configuration prompts (starting sector 0–31), respectively. No completed hardware/memory passes claimed. |

UNITEST and MOVI target processor RAM, sector handling, parity and ROM behavior;
PCMTEST is a pathological-case RAM test. They are not peripheral cards to add.
The 6600 instruction-test failures should be investigated as CPU/ROM compatibility first. A one-million-instruction KEYBOARD press followed by release and another 25 million instructions recovers the identified loader waits; the reason for this interaction is not yet established.
The menu repair alone cannot establish complete 6600 support.

### Additional 9370 surface verification

SURVAR's title says **2316**, and its observed geometry matches the existing
9370 implementation. This identification is an inference supported by the
successful run; a different physical pack drive is not inferred from the
filename alone.

Configuration: `cpu 5500`, initialized ROM, `disk-model 9370`, new temporary
logical drive 0, then write protection enabled and local-printer capture.
After loading, send `0`, `D`, `R`, ENTER, running 100,000 instructions after
individual keys. Run another 100 million instructions.

Result: `DISK PACK VERIFICATION COMPLETE`, `TOTAL BAD SECTORS = 0`, **97,440
sector reads**, **204 seeks**, cylinder maximum **202**, head maximum **19**,
sector maximum **23**, and **zero writes, formats or controller errors**.
This tests the existing 203-cylinder, 20-surface, 24-sector model over the whole
pack. It does not measure magnetic surface quality or validate formatting/
write-pattern mode, retry injection or the complete printed report.

## Peripheral implementation assessment

These are engineering judgments from the current device code, diagnostic text
and historical documentation. “High feasibility” means a bounded functional
model looks practical; it is not a time estimate or proof that every electrical
or maintenance test will pass.

### Existing console, RAM and cassette hardware

**Console CRT/keyboard: high feasibility; already implemented.** `TSTDIS`,
`TSTKEY`, `EXTDISPTEST`, KDTEST and processor-test console options exercise
address `0341` (`0xe1`). Existing regression covers character output, keyboard
latch, buttons, erase, scroll, fonts and lights. Extend coverage with the actual
programs, preserving old/new keyboard encodings, shifted punctuation,
backspace/cancel and button press/release behavior. Headless screenshots cannot
judge CRT alignment, cursor visibility or physical key switches. No new external
peripheral is required for EXTDISPTEST's demonstrated CRT mode.

**Cassette decks: high feasibility for record operations; moderate for faithful
timing diagnostics.** COPY, CTOS, EXRCASS and TAPTIM use the existing `0360`
(`0xf0`) device. Read/write records, deck selection and simulated-time callbacks
already exist. TAPTIM adds real requirements for forward/reverse transport,
rewind, end-of-tape, inter-record-gap timing, readiness and protection changes.
A `.tap` record image contains no measured tape speed or head alignment, so
speed/gap/endurance tests need an explicit virtual transport model rather than
instantaneous file positioning. Test COPY against disposable source/destination
fixtures before asserting a working copier. Investigate COPY06_1's halt and
loader/button waits separately; implementing another peripheral will not fix
an incomplete cassette prefix.

**Processor RAM/ROM: existing CPU work, not a peripheral project.** Preserve
PCMTEST as a useful long-running software check, examine TP1122 PUSH/POP output,
and obtain the correct 6600 firmware/environment before using TST6600C/D as
acceptance tests. UNITEST/MOVI also need meaningful sector mapping and parity
behavior. A model that simply suppresses parity faults cannot verify parity
fault detection.

### 9370/9374 disks and floppy boot

**High feasibility; largely present and exercised.** `Disk9370Device` at `0113`
(`0x4b`) supports selectable 9370/9374 geometry, controller buffers, logical
units, protection and asynchronous seek/transfer status. Existing tests cover
MD370, MD374, MA374 and SV374. SURVAR adds a full read-only 9370 pack traversal.
Next useful work is error injection for parity/CRC/seek failures and checking
retry/report behavior, not adding a duplicate disk controller. Alignment tests
can expose controller commands and status but cannot establish physical head
alignment on an emulated flat image.

DOSC_BOOT and both UBOOT tapes demonstrate the existing `0074` (`0x3c`) floppy
boot path. DOS.C is documented to use the 9380 family; these boot tapes are
useful integration fixtures, not new controller diagnostics.
[Datapoint DOS User's Guide, §2.2](https://www.bitsavers.org/pdf/datapoint/software/50432_DOS_Users_Guide_Version_2.5_May79.pdf).

### 9460/9462 multiport asynchronous communications

**Highest-value new peripheral; moderate implementation complexity.** MPXTEST,
BAUDRATE, TSTUBE and TSTUBE55 give complementary acceptance material. The
hardware supports eight independent full-duplex asynchronous ports; the
DATASHARE manual gives standard interface addresses `0151` and `0055` for
successive groups of eight. This is a different device from the existing
parallel-interface stub at `0226`.
[5500 DATASHARE 3 User's Guide, chapter 6](https://bitsavers.org/pdf/datapoint/software/50158_5500_Datashare_3_DS35500_Users_Guide_Version_1_Jul1975.pdf).

Start with configurable bus address decoding, port selection, per-port
transmit/receive state, character-ready/busy bits, baud scheduling and modem
controls. Add a selectable test-connector loopback mapping for MPXTEST's
RTS/CTS and DTR/DSR/carrier-type tests. Use simulated time for serial completion
so BAUDRATE measurements have meaning. Address-bit jumper tests deliberately
HALT four times and ask the operator to inspect an address LED; software tests
should record decoded/not-decoded state and resume deliberately.

First milestone: TSTUBE55 recognizes multiport 1 instead of reporting it
absent. Second: MPXTEST port loopback runs with verified bytes and control
signals. Third: connect a serial terminal, then exercise DATASHARE terminal
functions and baud measurement. Generic socket I/O without the hardware
register/status model would not satisfy these diagnostics.

### External terminals and serial printers

**High feasibility after the asynchronous interface exists; moderate protocol
work.** TSTUBE 1.A names 3360 and 3600 terminals. TSTUBE55 names 8200, 3601 and
9408 modem paths. PRINTERTEST targets serial printers attached directly or
through terminals/modems, with page width and CR/LF/FF behavior. A text-output
file alone will not model terminal printer selection, carrier and flow control.

Implement one documented terminal family first: receive/output queues,
cursor addressing, erase/roll, character mapping, bell and keyboard/INT input;
then add printer routing. A 3360 is a buffered serial terminal, so it should
not reuse the local console's bus device as though the interfaces were the
same. The 1973 catalog describes its terminal control functions.
[Datapoint Equipment Catalog, 3360 display unit](https://bitsavers.org/pdf/datapoint/2200/Datapoint_Equipment_Catalog_Sep1973.pdf).

EM3360 is software that emulates a 3360-102 using a processor. It is useful
application evidence, but is not itself proof that an external 3360 terminal
model passes TSTUBE. Its catalog identity is independently confirmed.
[Datapoint Software Catalog, emulator listing](https://bitsavers.org/pdf/datapoint/software/60000_Datapoint_Software_Catalog_Sep1982.pdf).

### 9900-404 / 9404 synchronous communications

**Feasible, moderate-to-substantial effort.** TST404 explicitly tests 9900-404,
address `0245`. EM3780B requires a 9404 communications adaptor and an external
modem; its trace program is supporting software for that emulator.
[Datapoint Software Catalog, IBM 3780 emulator/trace](https://bitsavers.org/pdf/datapoint/software/60000_Datapoint_Software_Catalog_Sep1982.pdf).

No synchronous device is registered at `0245` in the simulator. Implement
command/status/data registers, modem controls, receive/transmit clocks,
synchronization, character framing and hardware error-check behavior from the
actual hardware reference. TST404 should first pass local loopback and clock
options. Then add a deterministic binary-synchronous 3780 peer exercising
normal/transparent transfers and acknowledgement/error handling. A generic
asynchronous UART is not a substitute for this interface. The CTS lamp test
observed here is only an operator checkpoint, not a communications pass.

### Seven-/nine-track magnetic reel tape

**Feasible, moderate effort for a functional controller; more for fault tests.**
EXRIBM identifies the default I/O address as `0264` and offers block/filemark
advance/backspace, rewind, erase, buffer loading, reading and writing. This is
an external industry-compatible reel controller, distinct from internal
cassette decks. The 9550/9552-era family is a plausible hardware target;
identifying the exact controller revision still requires its register manual.
The contemporary catalog documents buffered seven-/nine-track units.
[Datapoint Equipment Catalog, tape systems](https://bitsavers.org/pdf/datapoint/60001-01_Datapoint_Equipment_Catalog_Nov1974.pdf).

Add a separate device with variable-length records, filemarks, BOT/EOT,
write protection, controller buffer addressing and command-completion events.
Use a reel-image format that preserves filemarks rather than reinterpreting
cassette numeric records as reel blocks. Parity, longitudinal check and CRC
status need injectable errors for diagnostic coverage. First acceptance test:
EXRIBM writes/rewinds/reads a known block and moves across a filemark on a
scratch image; then integrate EM3780's optional magnetic-tape input/output.
No such controller currently appears in `IOController`.

### ARC Resource Interface Module

See [RIM_FEASIBILITY.md](RIM_FEASIBILITY.md) for the subsequent 9483 specification
review, 5500/6600 setup probes, CPU integration gaps and transport proposal.

**Feasible, substantial effort; stage it as a two-node virtual network.**
RIMTEST's two node IDs/I/O addresses, SID/DID/data comparisons, transmitter
available/acknowledged, reconﬁguration, receive and parity statuses clearly
identify the ARC network RIM. Datapoint's ARC announcement defines RIM as the
processor's bus-connected Resource Interface Module.
[Datapoint ARC product announcement](https://bitsavers.org/pdf/datapoint/arcnet/Attached_Resource_Computer_System_Product_Annoucement_Dec1977.pdf).

Start with two RIM instances and documented buffer/command/status semantics,
node IDs, acknowledgement, receive availability, deterministic timeouts and
reconfiguration events. RIMTEST is better served by an in-process virtual
network than by a prematurely added host Ethernet bridge. Include explicit
fault injection for the parity/transfer errors it names. Model the period's
processor interface before substituting a later ARCNET controller register
layout. The test also names RAM DCL as a prerequisite; CPU/control-layer
support must be investigated before promising a full pass. Neither a RIM nor
that complete diagnostic environment was demonstrated here.

### Existing printer/parallel stubs and unrelated devices

Local-printer capture at `0303` (`0xc3`) already supports disk diagnostic reports.
The parallel adaptor at `0226` (`0x96`) and servo printer at `0132` (`0x5a`)
mostly return unsupported/no-op commands with zero status. Their class names
are not evidence of complete device emulation. A local print stream cannot
stand in for PRINTERTEST's serial printer/modem/terminal path.

`Disk9390Device` at `0161` (`0x71`) is also a stub; none of these tapes was
shown to be a dedicated test for it. Do not infer a requirement for this
controller from the presence of a stub or from a disk-sounding filename.

## Recommended order of work

1. PCMTEST and TSTDIS/TSTKEY now have regression checks in
   [test_console_diagnostics.py](../tests/test_console_diagnostics.py), run by
   `make -C dp2200sim test-console-diagnostics` and `test-headless`. These require
   a completed memory pass with zero counters, exact display/control states,
   and new-keyboard row/keypad comparisons plus error/retry behavior. Turn the
   SURVAR read-only pass into a regression check. Investigate the
   incomplete COPY06 prefix, COPY06_1/trace halts and the reason for collection loader/button waits.
2. Implement 9460/9462 register/status/timing and loopback. Use MPXTEST and
   TSTUBE55 hardware recognition as the first acceptance checks.
3. Add one external terminal family and serial-printer routing; exercise
   TSTUBE, PRINTERTEST and BAUDRATE against explicit configurations.
4. Add 9404 synchronous loopback and a scripted 3780 peer, or the reel tape
   controller if bulk tape operations are the immediate priority.
5. Implement paired RIMs and the required processor environment before ARC
   networking. Address 6600 ROM/CPU compatibility as a separate line of work.

The remaining uncertainties are explicit: exact business functions of the KE
applications, LGOPROG's intended image context, COPY06_1 and trace startup halts,
full cassette timing fidelity, physical terminal/printer behavior and complete
6600/RAM-DCL support. Successful initial loading does not resolve them.
