# Stage 2 virtual ARC link and TCP hub design

Updated 2026-10-07. The preferred network transport is now one unicast TCP
connection from each simulator to a central virtual hub. This supports a shared
ARC network across Internet-connected hosts with one configured server endpoint.
The local link, TCP hub and framing described here are proposals, not implemented
simulator features. Stage 1 still supplies only the RIM register/buffer interface.

## Protocol evidence and hardware revision

The [ARCNET Designer's Handbook, 61610-01 (1983)](https://bitsavers.org/pdf/datapoint/arcnet/61610-01_ARCNET_Designers_Handbook_1983.pdf)
is the principal reference for the link protocol: printed pages 2-3 through 2-8,
3-15 through 3-16, and Appendix A, A-3 through A-6. Chapter 6 describes extended
packets; those are outside the initial four-256-byte-page 9483 implementation.
Keep the [1981 RIM processor interface](RIM_STAGE1.md): the handbook's RIM chip
register interface is a different hardware interface, not a replacement for the
5500 EX/COM commands.

The protocol has five character-bearing transmissions and a reconfiguration
burst. All can be represented by one transport message per transmission:

| ARC event | Characters after the alert burst | Receiver action |
| --- | --- | --- |
| Invitation to transmit (ITT) | EOT, DID, DID | Addressed RIM takes the token; other RIMs observe line activity |
| Free buffer enquiry (FBE) | ENQ, DID, DID | Addressed RIM returns ACK if available, otherwise NAK |
| Packet (PAC) | SOH, SID, DID, DID, continuation pointer, 1–253 data bytes, two CRC bytes | Validate and receive; directed packet gets ACK; broadcast gets no ACK |
| ACK | ACK | Positive response to either FBE or PAC, distinguished by the pending exchange |
| NAK | NAK | Negative response to FBE; sender retries on a later token |
| Reconfiguration burst (RECON) | No character frame; repeated physical mark/space pattern | Interrupt ring operation and initiate ring reconstruction |

A packet is **never NAKed**. A rejected or corrupt packet produces no ACK.
For an unanswered enquiry or packet, the sender eventually sets TA without
TMA. An enquiry NAK keeps the packet pending. Packet ACK sets TMA, then TA.
On valid directed reception, ACK transmission precedes setting RI. TCP delivery
or a TCP acknowledgement must never count as an ARC packet acknowledgement.

The duplicated DID, continuation pointer and CRC belong to the ARC frame.
They must not be replaced by just an application payload. The physical alert,
character start elements and trailing silence can be derived from event type,
frame length and the selected hardware profile; individual dipulses do not need
TCP messages. Malformed physical framing would need a later explicit fault model.

The handbook also describes partial writes before validation, including SID
being written before destination checking. Receiving directly into the selected
RIM page must eventually account for those effects; a temporary-buffer-only
implementation would not reproduce every diagnostic observation.

## Shared medium and central server

A real ARC hub repeats a transmission to its other ports; it does not decode
addresses, grant tokens or invent ACKs (handbook 3-15). The virtual server also
orders and schedules events. That is a simulation service, not an ARC protocol
master replacing the RIM token state machines.

Use the same ARC event model for an in-process link and a TCP-backed link:

| Component | Responsibility |
| --- | --- |
| RIM device | Guest buffers, page registers, command/status behavior, receive validation and protocol replies |
| ARC link core | Token state, reconfiguration, medium occupancy, transaction state and simulated deadlines |
| In-process backend | Deterministic event delivery between the two diagnostic RIMs without sockets |
| Central TCP hub | Network membership, event ordering, virtual-time coordination and delivery to attached simulator connections |
| Client transport | Bounded framing, socket queues, connection lifecycle and emulator-thread event injection |

One connection can host several RIMs. Registration supplies a network name,
client session identity, hardware profile and node IDs; guest I/O masks are local
processor addresses and need not be sent to the hub. IDs 1–255 must be unique
within a virtual network; destination zero is broadcast. Reject duplicate IDs
without silently renumbering a running guest.

All members observe each committed line transmission, even if its destination
is another node. Only the appropriate RIM accepts a directed packet or responds.
For a simulator hosting two RIMs, the origin RIM observes transmission completion
and the other RIM observes reception. The origin must not process a reflected
packet as reception. A TCP connection is a transport boundary, not a physical
single-port hub model; the server must preserve these per-RIM distinctions.

Initially, use the hub path for all RIMs attached to a TCP network, including two
RIMs in one simulator. Avoid a simultaneous local fast path, which could deliver
a message twice or disagree with hub ordering. The separate local-only backend
remains available for fast diagnostic regression.

Join, departure and restart are membership changes that trigger the modeled
reconfiguration process. They must not merely replace the next-ID table with a
sorted member list: token discovery, unanswered invitations and RECON status
are guest-observable behaviors. Invalid simultaneous transmissions should be
rejected or handled by an explicit collision/fault policy, not silently queued
as valid token use.

## Proposed TCP framing, version 1

TCP is an ordered byte stream. A read can return part of one message or several
messages, irrespective of how the sender called write.
[RFC 9293](https://www.rfc-editor.org/rfc/rfc9293.html)

Use an unsigned 32-bit big-endian body length, followed by a fixed 44-byte header
and its payload. The length excludes its own four bytes and includes the header.
Version 1 accepts body lengths 44–4096; a standard ARC packet carries at most
260 payload bytes. These limits and field assignments are a proposed simulator
protocol, not a historical ARCNET transport standard.

| Header offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | Magic: ASCII `DARC` |
| 4 | 1 | Protocol version: 1 |
| 5 | 1 | Message type |
| 6 | 2 | Flags; initially zero |
| 8 | 8 | Hub network epoch |
| 16 | 8 | Hub event sequence; zero for an uncommitted proposal |
| 24 | 8 | Virtual event start time in nanoseconds |
| 32 | 1 | Origin RIM ID; zero for connection-level control |
| 33 | 1 | Target RIM ID; zero for broadcast or connection-level control |
| 34 | 1 | Reply phase: none=0, enquiry=1, packet=2 |
| 35 | 1 | Reserved; zero |
| 36 | 8 | Exchange ID; zero when no exchange applies |
| 44 | variable | Payload |

Serialize integers explicitly in network byte order; never transmit a C++
structure using sizeof. ARC packet CRC bytes retain their ARC-defined encoding.
The payload size is body length minus 44, so no second length field is needed.

Proposed types:

| Value | Type | Payload/purpose |
| --- | --- | --- |
| 0x01 | HELLO | Bounded UTF-8 JSON registration: network, session, node IDs, profile, clock mode |
| 0x02 | WELCOME | Bounded UTF-8 JSON negotiation, accepted membership and epoch |
| 0x03 | ERROR | Bounded UTF-8 error reason; fatal violations close the connection |
| 0x04 | LEAVE | Explicit departure of the connection's nodes |
| 0x05 | GRANT | Hub permission to advance to the header's virtual-time horizon |
| 0x06 | ADVANCED | Client reports completion of a grant and its ordered local proposals |
| 0x07 | APPLIED | Client confirms committed event applied; identifies its event sequence |
| 0x08 / 0x09 | PING / PONG | Host-time connection health; unrelated to ARC acknowledgement |
| 0x20 | ITT | Three raw ARC characters |
| 0x21 | FBE | Three raw ARC characters |
| 0x22 | PAC | 8–260 raw ARC characters, including SOH and CRC |
| 0x23 | ACK | One raw ARC character |
| 0x24 | NAK | One raw ARC character; enquiry phase only |
| 0x25 | RECON | Empty; waveform and duration derived from negotiated profile |

The handshake schema and detailed clock-grant algorithm still need specification
before implementing a compatible server/client pair. The reviewed handbook
sections do not establish the exact CRC polynomial, initialization or bit/byte
ordering; verify those from controller documentation before claiming raw-frame
compatibility. Additional client command
and cancellation messages will be needed if protocol state is moved to the hub.
For the initial design, the RIM state machines remain client-side.

Origin, target and reply phase in the transport header are metadata. Physical
ACK/NAK frames have no addresses. Correlate responses using epoch, origin and
exchange ID plus phase; the enquiry ACK must not complete the later packet.
Each new guest attempt receives a new exchange ID. Reconnection invalidates
outstanding work; hub restart creates a new epoch and forces reconfiguration.
A guest retry must remain distinct from accidental transport replay.

The hub assigns a single increasing sequence to committed ARC events and sends
the same ordered event stream to every member. TCP only orders bytes within
one connection; it supplies no common order across multiple clients by itself.
Validate that a proposing connection owns the origin ID, that its token/response
state permits the event, and that envelope metadata agrees with the raw frame.
CRC-invalid frames can remain admissible under explicit fault injection so that
receiver validation is testable.

Decoder requirements: retain partial prefixes/bodies, extract all complete frames,
reject invalid lengths before allocating, validate types and payload sizes, and
bound queued output. Handle partial writes, disconnect mid-frame and EOF.
Backpressure must stall coordinated simulation or detach a failed connection
under a stated policy; silently dropping ARC events would corrupt ring state.

## Timing over the Internet

The handbook's Appendix A gives a 74.6 microsecond response timeout and a
12.6 microsecond turnaround. ITT/FBE last 15.6 microseconds, ACK/NAK last 6.8,
and a standard packet lasts `33.2 + 4.4 × data_length` microseconds. Those are
simulated line times, never TCP socket timeouts.

The 1983 description uses approximately 840 ms for lost-token detection and
765 repetitions of eight marks plus one space for RECON, unlike the earlier
FRIL figures. These values are candidates for a later protocol profile, not
proof that every 1981 9483 revision uses them. Keep revision-dependent constants
in one profile and record which values RIMTEST actually depends on.

For a diagnostic-faithful Internet mode, use a hub-coordinated conservative
virtual clock. Clients may execute only up to a granted horizon, report their
progress and locally generated events, then wait. The hub commits the next
medium event only when no participant can still produce an earlier event; all
clients apply it at the agreed virtual boundary before advancing past dependent
responses or timeouts. An unanswered enquiry times out after 74.6 simulated
microseconds because there is no scheduled response, not because a remote TCP
read has waited 74.6 wall-clock microseconds.

This requires synchronizing CPU-visible RIM commands as well as token events.
Sequencing already-late packets at the server cannot undo a guest timeout.
CPU instructions span time intervals, so the grant algorithm must define event
application at instruction boundaries, permissible overshoot and tie-breaking.
Zero-lookahead or tiny instruction-sized grants can be correct but prohibitively
slow over the Internet. Batching independent work and idle token cycles must be
measured before promising usable interactive speed. Do not skip a token cycle
that could observe an intervening guest command.

A separately named functional mode could accept coarser timing and paced guest
execution, but it would need explicit timing limitations and must not be used
to claim diagnostic timing fidelity. Start with the deterministic local backend;
then a slow coordinated TCP mode establishes correctness before optimization.

CPU halt and debugger pause are distinct. A halted CPU may leave hardware RIM
activity running. A paused participating simulator should initially pause the
whole coordinated network, with a visible status; detachment is an explicit
alternative that initiates reconfiguration. The headless runner currently blocks
on stdin between commands and needs an event loop. Socket reception should queue
work, with guest buffer/status changes applied only on the emulator thread.
Reset/disable callbacks need generation checks so old events cannot complete a
new transfer.

## Configuration and implementation gates

A remote simulator needs one hub endpoint and a network name. A server can host
several independent networks. Outbound client TCP connections avoid requiring
an inbound listener at every simulator; the hub must be reachable and configured
to accept them. TLS and access control should be provided for an Internet-facing
service. No virtual cable topology, hub port count or per-peer address list is
needed for the functional shared-medium model.

1. Implement the deterministic local link: ITT, FBE, PAC, ACK, NAK and RECON;
   directed/broadcast reception, busy/absent receivers, cancellation and reset.
2. Establish original RIMTEST transfer progression and meaningful native
   regressions; document any parity or timing limitations.
3. Implement and test the standalone frame codec and bounded hub relay:
   every split point, coalesced frames, malformed lengths/types, partial writes,
   disconnects and common event order across three clients.
4. Specify and implement clock grants and client integration. Verify that added
   host latency changes wall-clock runtime without changing simulated results.
   Cover two RIMs in one client, duplicate IDs, reconnect, slow clients and pause.
5. Exercise multiple physical hosts and Internet connections; optimize batching
   only while preserving those checks.

## Earlier multicast feasibility result

The earlier multicast investigation remains useful evidence for an optional LAN
backend. [probe_arc_multicast.py](../tests/probe_arc_multicast.py) and its
[captured macOS result](arc-multicast-probe-macos.json) tested three processes,
20 messages each, common group/port and local loopback: all three received all
60 messages, with no missing, duplicate or invalid payloads observed.
The test used loopback, TTL 0 and SO_REUSEADDR/SO_REUSEPORT. It proves that
same-host transport configuration worked, not a RIM transfer or Internet routing.
TCP to a central hub is now the preferred cross-host architecture.
