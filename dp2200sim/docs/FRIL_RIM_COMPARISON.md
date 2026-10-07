# FRIL and RIM documentation comparison

Reviewed 2026-10-07. Sources:

- [FRIL Adaptor Specification, 24 February 1977](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/arcnet/FRIL_Adapter_Feb77.pdf).
  Scanned document: 23 PDF pages, including protocol diagrams and event notes.
  Page references below identify PDF pages because printed pagination is irregular.
- [9483 Resource Interface Module specification, 20 July 1981](https://bitsavers.org/pdf/datapoint/arcnet/60911_9483_Resource_Interface_Module_Product_Specifications_19810720.pdf).

## Relationship

FRIL expands to Fast Resource Intercom Link (section 1, PDF p. 4). Its memory,
commands, packet layout and configured addresses closely match the later RIM.
This strongly supports FRIL being an earlier version/name of the same interface
family. Neither specification explicitly records a rename, and they document
some different capabilities; they should not be treated as identical revisions.

| Feature | February 1977 FRIL | July 1981 RIM |
| --- | --- | --- |
| Buffer | Four 256-byte pages, independent processor/TX/RX page registers | Same |
| Buffer access | EX ADDRESS/STATUS/DATA, INPUT/PIN/MIN, WRITE/MOUT, COM4 byte pointer | Same |
| COM1 commands 0–6 | Clear IPE, disable TX/RX, select processor page, enable TX/RX, clear POR | Same |
| Status bit 2 | Unassigned, always zero | RECON |
| COM1 command 7 | Not listed | Clear RECON |
| Factory I/O address | Octal `0234` | Same |
| Additional module addresses | `0232`, `0231`, `0254`, `0252`, `0251` | Same |
| Processor bus described | 2200 compatible | 5500 compatible |
| Network | 2.5 Mbit/s coax, unique IDs, token passing, directed packets and broadcast | Same basic model |

The status diagram and command list were visually checked on PDF pp. 9–11.
Since RIMTEST names RECON, retain the later RIM register definition as the
simulator target. The earlier document broadens the historical bus compatibility
evidence, but does not establish that RIMTEST or the emulator works on a 2200.

## Addressing and the diagnostic fixture

FRIL section 7.2 (PDF p. 16) reserves the alternative addresses for separate
modules hosted by one processor. Section 2.2 allows two modules to connect
directly over coax. This independently supports the two-module RIMTEST fixture.
The document supplies no left/right terminology or separate TX/RX bus addresses.

Its EX ADDRESS description (PDF p. 9) requires the proper address and enters
STATUS mode, but supplies no decoder schematic or explicit alias predicate.
It therefore does not independently prove the `0234`/`0235` alias behavior.
That conclusion still rests on RIMTEST's executed bit checks and the processor
bus decoder architecture described in the
[addressing investigation](RIM_ADDRESSING_INVESTIGATION.md).

## Additional evidence for the local network implementation

Sections 3.2, 3.3 and 3.6 add considerably more protocol detail than the short
1981 product specification. Important candidate timings and events include:

| Event | FRIL description | Location |
| --- | --- | --- |
| Lost token | Reconfigure after approximately 500 ms without an invitation | PDF p. 8 |
| Reconfiguration | Repeated burst; idle detection at 78.2 microseconds | PDF p. 8 |
| Election | Wait `146 microseconds × (255 − ID)`; highest ID starts first | PDF p. 8 |
| Token handoff | If no activity within 74.7 microseconds, increment next ID and retry | PDF p. 8 |
| Enquiry/packet response | Wait up to 75.1 microseconds for a reply | PDF p. 13 |
| Broadcast completion | Wait 14.8 microseconds before passing the token | Figure 3-4, PDF p. 23 |

These are 1977 values to cross-check against later hardware documentation before
claiming exact 9483 timing. They provide concrete simulated-time events for
Stage 2; host UDP round-trip time should not drive these microsecond deadlines.

The data-exchange description also clarifies failure transitions: a busy
receiver returns NAK, so the sender retains its pending packet and retries on
a subsequent token. No enquiry response instead completes the send attempt
with TA set. Missing packet ACK sets TA without TMA; receipt can therefore
precede a sender-side acknowledgement failure. Broadcast completes without
an ACK. A transmit count changed to zero can abort an in-progress packet.
These distinctions should guide network failure regressions rather than treating
all unacknowledged sends as one retry case.

No emulator behavior was changed for this comparison. The new document is
useful protocol evidence, while the later RIM specification remains the
register-interface target.
