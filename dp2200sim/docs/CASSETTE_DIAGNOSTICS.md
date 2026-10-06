# Cassette verification with EXRCASS and TAPTIM

Tested 2026-10-06 using the unmodified `EXRCASS_V1.1.tap` and
`TAPTIM_V1.5.tap` images on the default 2200 CPU.

**Record transport and bounded write/read checks pass on both decks. TAPTIM's
hardware timing tolerances fail.** This is partial cassette verification, not
certification of the entire timing/endurance suite.

The [measured results](cassette-diagnostic-results.json) contain diagnostic
SHA-256 hashes, deck positions/status, written record lengths, endurance read
counts and the actual timing table. Source tapes are read-only; all writable
media are new temporary files.

## Reproduce

From the repository root:

```sh
make -C dp2200sim test-cassettes
```

This runs the functional regressions, including both original diagnostics and
native-instruction checks for empty reads, one-byte forward/reverse records,
write/readback and rewriting a shorter record.

For the complete report, including the known timing failure:

```sh
python3 dp2200sim/tests/cassette_diagnostics.py \
    --output /tmp/cassette-diagnostic-results.json --require-timing
```

The strict command writes the report and exits **1** because the timing rows
are outside tolerance. `make -C dp2200sim test-cassette-timing` runs the same
strict check and writes `dp2200sim/cassette-results.json`. Omitting
`--require-timing` reports the same failures but exits successfully if the
functional checks complete. It does not relabel timing as passed.

The full `make -C dp2200sim test-headless` suite includes these functional
regressions. `make -C dp2200sim all test-headless` completed successfully with
all 36 tests passing after these fixes. The strict diagnostic runner completed
its functional checks and returned the expected exit status 1 for timing failures.
The test code is in [cassette_diagnostics.py](../tests/cassette_diagnostics.py),
[test_cassette_diagnostics.py](../tests/test_cassette_diagnostics.py) and
[test_cassette_write.py](../tests/test_cassette_write.py).

## EXRCASS transport checks

After loading the real diagnostic and reaching its menu, replace both decks
with read-only fixture tapes. Each contains eight repetitions of three records:
one byte, all 256 byte values, then one byte. This exercises one-byte boundaries
and differing byte values as well as ordinary longer records.

For **each deck**, drive the diagnostic using its own commands:

| Command/check | Verified result |
| --- | --- |
| `1` or `2` | Correct rear/front deck selected. |
| `F` | File position advances and transport is running. |
| `S` while moving | Pending transport stops; further CPU execution does not change tape position. |
| `F` through the whole image | Stops at exact file end with END-OF-TAPE and DECK-READY set. |
| `B` through the whole image | Stops at byte zero with endpoint and ready status set. |
| `R` after movement and STOP | Rewind completes at byte zero, with no pending transport. |
| Deck isolation | The unselected deck does not move. |
| Image integrity | Both read-only fixture files remain byte-for-byte unchanged. |

Each fixture is 2,256 framed bytes. STOP is checked while the transport is
running, rather than only after reaching an endpoint. The fixture bytes are
independently checked; a menu highlight alone is not the success criterion.

Reverse cassette bytes have their bits reversed by the device. A separate
native-instruction regression reads `96 35` from two one-byte records forward,
then verifies `AC 69` in reverse order when backspacing. This distinguishes
correct reverse data from mere reverse file-position movement.

## TAPTIM data checks

Load TAPTIM, stop the diagnostic cassette, then replace both decks with new
writable scratch images. ENTER runs its initialization; reaching `NEXT TEST?`
is required before selecting a test. This also exercises the no-data write
cycle that previously hung the emulator.

### T1 and T2

Run each timing loop independently, until the display enters pass 2. The test
must remain running, consume commands, provide all seven measured rows and
produce valid 600-byte and 100-byte records consisting of `FF`. These record
patterns and their framing are inspected independently of the screen.

This proves the loop executes and records its test data; its printed timing
errors are still failures. On deck 2, a one-byte `FF` initialization record is
also present before the timing records.

### E1 and E2: bounded endurance

Run each deck's endurance diagnostic for **25 million instructions** after
selection, sampling the screen every million instructions. Reject HALT,
read/write failure messages and mismatch reports. Capture actual cassette
`INPUT` data through `io-trace`, rather than inferring reading from a file's
existence.

Both runs read **18,786 bytes** and leave **120 valid records** in the tested
image. Record sizes include **1, 3, 5, 7, 9, 15, 25, 51, 101, 151, 201 and 255
bytes**. Independently inspect every retained record against TAPTIM's alternating
`FF/00` pattern, starting with `FF`. Both `377` and `000` are observed in the
read trace, and no diagnostic data error is displayed during the bounded run.
The simulator reaches approximately 197 seconds of accumulated simulated time,
including loading and initialization.

These checks cover repeated writing, reading and changing record sizes on both
decks, including rewriting records. They are not a whole physical-tape endurance
pass: the diagnostic still shows pass 1, and the image model has no configured
300-foot tape capacity or physical wear/fault model.

## Actual timing result

Both T1 and T2 display the same measurements after the first loop. These are
numbers as printed by the diagnostic, without a conversion to other units.

| Measurement | Nominal | Tolerance | Measured, both decks | Result |
| --- | ---: | ---: | ---: | --- |
| Modulator delay | 100 | ±10 | 0 | Fail |
| Pre-write | 170 | ±17 | 0 | Fail |
| Post-write | 140 | ±14 | 2 | Fail |
| Pre-read | 75 | ±8 | 0 | Fail |
| Reverse post-read | 140 | ±14 | 4 | Fail |
| Forward post-read | 140 | ±14 | 4 | Fail |
| Slew | 210 | ±21 | 71 | Fail |

The diagnostic increments each row's error count to 1 and marks it with `*`.
The comparison in the runner uses both the displayed tolerance and the error
counter. A test that only looked for the timing table would incorrectly pass.

The current controller schedules a 70-ms record-gap delay and 2.8-ms byte
intervals, with short completion delays. WBK exposes write readiness immediately;
RBK/SF clear the gap flag when issued. Rewind positions the image immediately
and reports completion after 1 ms. Those simplified status transitions do not
model the modulator/demodulator startup and shutdown delays measured by TAPTIM.
The timing failures are consistent with those simplifications. Matching every
row requires a documented transport phase/timing model, rather than changing
nominal values or suppressing the diagnostic's error output.

### IRG and full-length speed

I1 was also attempted on a newly created scratch image and did not establish a
completed inter-record-gap measurement. Its proper fixture/preconditions and
the relationship to the endurance data need further work.

S1 expects a **300-foot tape taking 480 seconds**. On an empty record image it
prints forward time `000`, `-480 ERROR` and remains in the speed screen. This is
not a valid physical speed calibration fixture. `.tap` framing contains records,
not remaining blank-tape length; the emulator does not currently supply the
required transport-length model. Full-length reverse speed is also unverified.

The diagnostic's purpose as a cassette endurance/timing test is independently
listed in Datapoint's contemporary
[Software and Documentation Schedule](https://ftpmirror.your.org/pub/misc/bitsavers/pdf/datapoint/software/60231_Software_and_Documentation_Schedule_Dec75.pdf).
That listing concerns release 1.6; the measurements above come from the actual
1.5 tape in this repository, not assumed equivalence between revisions.

## Fixes made during verification

The original tests found concrete functional faults, addressed without changing
the diagnostic tapes or their tolerances:

- **WBK without data:** could leave the controller permanently busy. The
  existing idle-write timeout now also finishes a no-data write cycle without
  emitting an empty record.
- **Empty tape/EOF:** unchecked reads could expose an uninitialized byte or
  keep scheduling reads after EOF. Endpoint without data is now distinguished
  from a valid last byte and from an absent cassette.
- **One-byte records:** the first-byte path skipped record completion, so it
  could treat framing as data or continue across a block that should stop.
  First/final bytes now use the same gap/ready/endpoint completion path.
- **Read/write stream switching and shorter rewrites:** buffered positioning
  could append instead of rewriting; shorter rewrites could leave stale framed
  data behind. Writes synchronize the stream position and truncate the image's
  remaining record sequence after the new record. This is the record-image
  overwrite policy, not a model of partially overwritten magnetic flux.

New regressions check both the real diagnostic interactions and these boundary
cases. A read-only `cassette-state` headless command exposes selected deck,
controller status, pending transport, and per-deck file position/presence/
protection. It does not consume a data byte or execute instructions.

Further verification should focus on TAPTIM-compatible startup/shutdown status
and timing, IRG fixtures, explicit virtual tape capacity, and write-protection/
missing-cassette fault cases. Passing EXRCASS transport and the bounded data
loops does not establish those behaviors.
