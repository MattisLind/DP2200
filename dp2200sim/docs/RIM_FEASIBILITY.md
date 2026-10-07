# RIM feasibility and virtual ARC network

Assessed 2026-10-06; updated 2026-10-07. The
[Stage 1 register/buffer implementation](RIM_STAGE1.md) is now available.
The network portions below remain proposals; RIMTEST has not passed. A
[Stage 2 TCP hub design](RIM_STAGE2_DESIGN.md) now specifies proposed framing,
ARC control events and coordinated virtual time using the 1983 Designer's
Handbook. The earlier successful multicast probe remains recorded as an optional
LAN experiment; no simulator network backend is implemented yet.

**Adding a 9483 RIM is feasible. Start with the 5500 and two RIMs on an
in-process network, then add unicast TCP to a central virtual hub for Internet
connections.** Each remote simulator configures one server endpoint and network
name. The substantial work is the RIM state machine and coordinated simulated
time, rather than opening a socket.

## Processor compatibility and the right hardware target

The earlier [February 1977 FRIL specification](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/arcnet/FRIL_Adapter_Feb77.pdf)
describes a closely matching predecessor interface and adds token/retry timing
details. See the [FRIL/RIM comparison](FRIL_RIM_COMPARISON.md). Its unused
status bit 2 differs from the later RIM's RECON bit, so use it as protocol
evidence while retaining the 1981 RIM register interface.

The 1977 [ARC Systems Catalog](https://bitsavers.org/pdf/datapoint/arcnet/60530_ARC_Systems_Catalog_Nov1977.pdf),
pp. 2–3, lists both 5500 and 6600 systems, as well as other ARC processors.
RIM support was therefore not exclusive to the 5500. Target the 5500 first in
this simulator; treat 6600 support as a separate acceptance milestone.

The [9483 product specification, 60911](https://bitsavers.org/pdf/datapoint/arcnet/60911_9483_Resource_Interface_Module_Product_Specifications_19810720.pdf),
sections 1, 3 and 7, supplies the original bus interface:

- Requires a 5500-compatible I/O bus.
- Four 256-byte buffers; independent processor, transmit and receive pages.
- Byte address increments and wraps within its page.
- `EX ADR/STATUS/DATA`, `INPUT/PIN/MIN`, `EX WRITE/MOUT`; `COM4` selects byte
  address and DATA mode.
- `COM1`: `000` clear IPE, `001` disable transmit, `002` disable receive,
  `(page << 3) | 003/004/005` select processor page/transmit/receive,
  `006` clear POR, `007` clear RECON. Numbers are octal.
- Status bits 0–7: TA, TMA, RECON, TPE, POR, DA, IPE, RI; DA is always set.
- Factory address `0234`; additional addresses `0232`, `0231`, `0254`, `0252`,
  `0251`. Node IDs 1–255; destination zero means broadcast.
- Buffer bytes 0/1/2 contain SID/DID/count; data occupies the tail. Normal
  lengths are 1–253; count values 1/2 have special full-length semantics.
- Busy destination: retry on later token opportunities. TMA means receiver
  hardware acknowledged, independently of guest software processing.

The later [ARCNET Designer's Handbook](https://bitsavers.org/pdf/datapoint/arcnet/61610-01_ARCNET_Designers_Handbook_1983.pdf),
chapter 4, describes a chip with TEST and extended-timeout status bits in
positions where RIMTEST expects parity/device status. Its chip register layout
must not be substituted for the 9483 bus interface. Chapter 2 is useful for
token, enquiry, ACK/NAK, CRC and reconfiguration behavior. It describes
microsecond response windows; these should be modeled as virtual link events,
not implemented as operating-system socket deadlines.

## What the existing diagnostic and code establish

Fresh bounded runs of the unmodified `RIMTEST.tap` are recorded in
[rim-feasibility-probes.json](rim-feasibility-probes.json), including its hash,
commands and screens:

| CPU | Observation without a RIM |
| --- | --- |
| 2200 | Blank screen and CPU halt. |
| 5500 | Prompts for left/right IDs. Entering IDs 1/2 reaches the left I/O address prompt; entering `234` produces `I/O Addressing Failure`. |
| 6600 | Same setup progression and missing-device failure as 5500. |

This demonstrates setup execution on the advanced CPUs, not successful
RIM transfers or full CPU compatibility. The supplied tape's embedded labels
include TA/TMA/RECON/TPE/POR/DA/IPE/RI, SID/DID/data and byte-count comparisons,
receive timeouts, input/output parity interrupts, and `RAM DCL REQUIRED`.
Those are useful acceptance requirements. Determine the RAM DCL check's exact
meaning and triggering code before treating it as a missing processor feature.
The screen device already advertises RAM display status; this probe did not
reach a RAM DCL failure.

Follow-up with attached Stage 1 RIMs identifies the addressing failure as a
missing strap-decoder behavior: RIMTEST first probes `0235` and expects DA
from the RIM at `0234`. Temporary alias responders allow both address checks
to complete and reach the main test display. See the
[addressing investigation](RIM_ADDRESSING_INVESTIGATION.md) for instruction
traces, the complete bit-test routine and the limits of this experiment.
The current mask decoder replaces the temporary two-address model. Real RIMs
at `0234` and `0232` complete both address checks and reach the main diagnostic
display. The same setup checks pass with all six reserved modules attached.
Packet/token completion still requires Stage 2. Blank/zero right inputs do not
bypass the diagnostic's second-module requirement.

Code inspection reveals these concrete integration points:

- [IOController](../dp2200_io_sim.cpp) owns an address-indexed device table and
  forwards `INPUT`, `WRITE` and `COM1..4`. Optional independently addressed
  RIM devices are now available. Sharing a virtual link remains Stage 2.
- [CPU implementation](../dp2200_cpu_sim.cpp) has `PIN` input-parity handling
  and existing MIN/MOUT handling in `executeExtended`. The initial assessment
  mistakenly inspected obsolete branches in `immediateplus` and reported
  MOUT as unimplemented. Stage 1 verifies the active decoder's transfers,
  registers and flags, adds variable per-byte timing, and propagates transfer
  failures. Full parity fault behavior still belongs to a later milestone.
- The CPU has an input-parity fault path but no corresponding output-parity
  failure state/dispatch path. RIMTEST names both. Device IPE/TPE flags and
  CPU bus-parity traps are different mechanisms and need separate tests.
- Existing callback queues provide simulated-time scheduling in both runners.
  Use them for the device and local link; do not make RIM status depend on
  Python or UI polling speed.
- RIMTEST wants two actual controllers, called left and right, on the same
  processor. Two local RIMs with different IDs are the simplest initial test
  fixture; no second emulator process is required.

The specification is sufficient to begin a credible functional device. Exact
reset timing, unusual counts, diagnostic prerequisites and parity paths still
need targeted instruction tracing and, where necessary, maintenance documents.
Do not make DA/TMA/RI permanently true merely to get past the diagnostic.

## Choosing a transport with little configuration

The preferred cross-host transport is now TCP to a central server, following the
request to connect virtual 5500s over the Internet. The earlier preference for
automatic multicast LAN discovery is superseded. See
[RIM_STAGE2_DESIGN.md](RIM_STAGE2_DESIGN.md) for framing and timing details.

| Transport | Configuration | Assessment |
| --- | --- | --- |
| In-process virtual network | Local diagnostic fixture with fixed IDs | Initial implementation: one clock, repeatable ordering and injectable faults. |
| Central TCP hub | One reachable endpoint and network name | Preferred cross-host backend; framed ARC events, shared ordering and virtual-time coordination. |
| UDP multicast | Common group/port and interface selection | Optional LAN backend. Same-host three-process fan-out passed on macOS; Internet routing and a common simulation clock are not supplied by group membership. |
| UDP broadcast | Broadcast-capable interface/subnet | Local fallback with discovery and reliability limitations. |

The TCP server supplies the shared event order across clients. TCP's reliability
within each connection does not establish that order or make its ACK equivalent
to a RIM acceptance. The hub must remain separate from the ARC protocol state
machines, and virtual time must account for host scheduling and Internet latency.
Guest ARC software may still require file-processor identities and application
configuration; transport registration cannot provide those operating-system
services.

## Proposed device and network split

Keep three components independent:

1. `RimDevice`: guest buffers, page/address registers, command decoding,
   status, reset and parity behavior.
2. `ArcNetwork`: membership, link arbitration, receiver availability,
   packet acceptance and scheduled completion/reconfiguration events.
3. `ArcTransport`: in-process delivery first; optional socket transport later.

Network names separate independent emulated ARC networks. For two diagnostic
RIMs use stable IDs 1 and 2 at addresses `0234` and `0232`. These are proposed
test fixtures, not newly available simulator commands.

For TCP registration, send protocol version, network name, session identity,
hardware profile and hosted node IDs. The hub validates unique membership,
assigns a network epoch and orders committed ARC events. One connection can
host several RIMs. Reconnection invalidates pending exchanges and initiates
modeled reconfiguration. Use explicit length framing for ITT, FBE, packet,
ACK, NAK and RECON messages; see the Stage 2 header proposal.

Node identity is distinct from host IP and TCP endpoint. Offer a persisted automatic
ID for convenient new instances, but detect duplicates and allow explicit IDs.
Do not silently change a running guest's configured node identity. IDs must be
fixed in regression tests and may need to match installed ARC software.

Unicast acceptance must come from the destination RIM after validating and
placing the packet in its enabled receive buffer. A successful socket write, TCP ACK or
peer heartbeat must not set TMA. Represent busy, absent and accepted outcomes
separately. Sequence/epoch handling must prevent duplicate transport delivery
from producing duplicate guest packets, while keeping guest retransmissions
distinct. Broadcast acceptance and acknowledgement policy belong to the RIM
model, independently of whether host delivery uses multicast or unicast.

ARC token and reply events can be sent over TCP, but their microsecond deadlines
must use simulated time. A fast guest can otherwise time out before a remote
host is scheduled. Diagnostic fidelity across processes requires hub-coordinated
virtual-time grants and event application, including CPU-visible RIM commands.
Sequencing socket messages alone is insufficient. A conservative coordinated
mode may run slowly over the Internet; batching is a later optimization.

Queue socket input and apply buffer/status changes on the emulator thread.
Define pause, halted-CPU, reconnect and slow-client behavior. The local backend
retains fast deterministic execution without a server or live network access.

There is no need to expose active/passive hubs as configurable devices for
functional networking. A logical shared link can deliver traffic among members;
hub port counts, cable lengths and propagation become optional fidelity features.
Removing physical hubs does not remove the need for arbitration, membership
and correct acknowledgements.

## Implementation order and acceptance gates

| Stage | Deliverable | Required evidence |
| --- | --- | --- |
| 1 | 5500 RIM register/buffer model and required CPU transfers | Test all pages, independent page registers, pointer wrap, modes, status/reset commands and MIN/MOUT behavior. |
| 2 | Two-node deterministic local network | Addressed and broadcast transfers, correct SID/DID/count/data, receive-inhibited behavior, absent destination, cancellation and reconfiguration. |
| 3 | Original RIMTEST regression | Both controllers recognized; resolve RAM DCL check; require completed diagnostic passes with zero transfer errors. Test parity flags/traps through deliberate fault injection. |
| 4 | Central TCP hub and coordinated clock | Framing under split/coalesced reads, common event order, two processes then two hosts, duplicate IDs, reconnect, busy receiver, slow clients, pause/resume and injected host latency. |
| 5 | Guest ARC software and 6600 support | Demonstrate actual guest-to-guest service use and independently verify CPU/model behavior. A socket packet exchange alone is insufficient. |

Effort is moderate for the basic 9483 buffer/register device, substantial for
full RIMTEST coverage with CPU parity and timing fidelity, and additional work
for coordinated Internet interoperability. Precise scheduling estimates would be
premature until RIMTEST's prerequisite/transfer paths are traced.
Stage 1 is available; the next implementation is the deterministic local link.
Choosing a TCP hub does not require coupling local diagnostic regressions to
live network access. The framing and clock design remain proposals.
