# Architecture

The application composes four independently testable responsibilities:

1. **Discovery** parses advertisements, collects bounded owned candidates and
   selects a unique or explicitly named logger. Hints do not prove a model.
2. **BLE session/adapter** owns connection lifecycle, subscription, deadlines
   and one in-flight read. The adapter uses the same controller for discovery;
   scanning and GATT are mutually exclusive.
3. **Protocol codecs** construct FC03/FC04 reads and validate identity,
   correlation, lengths, padding and CRCs before returning registers.
4. **Profiles/experimental catalogue** interpret validated register blocks.
   A shared meaning-free decoder retains original words and integer values.
   Profiles also derive bounded read ranges from named signals without I/O.

Applications own Wi-Fi, persistence, polling cadence, reconnect policy, profile
choice, caching, publication and automation. None of these responsibilities
are imported into the protocol core. A standalone asynchronous RS-485 client
and automatic model/profile resolution are not yet implemented.

## Invariants

- Incomplete or ambiguous discovery must never become an implicit target.
- Advertisement names/UUIDs are hints, not authenticated identities. GATT
  service/characteristic checks and encrypted response validation still apply.
- A confirmed lifecycle barrier precedes a new scan or connection. Missing
  lifecycle evidence is a terminal fault, not permission to restart or retry.
- Results own their data; new reads/scans do not mutate previously taken values.
- CRC success, sample freshness and physical interpretation are distinct facts.
- Unknown/out-of-range data fails explicitly; missing data is not a zero sample.
- Experimental evidence stays attached to raw and converted values.
- Multiword values are decoded with explicit signedness and word order.
- The library performs no inverter configuration/register writes or reboot.
- Runtime, examples and tests never depend on research directories or secrets.

## Boundaries

The portable C++11 session uses a narrow `BleLink` technology contract. The
Arduino-ESP32 2.x implementation owns Bluedroid and bounded callback queues.
Other Bluetooth stacks must not share this controller instance. The public
`BleScanner` contract exposes a finite asynchronous scan with an owned result;
its concrete implementation lives with the same controller owner.

Profiles cannot initiate I/O. Raw values may be reinterpreted without changing
the transport. The experimental catalogue requires explicit opt-in and does not
silently extend or redefine normal signals. Numeric inverter codes retain
category/subcode; the library intentionally has no code-description dictionary.

The small `signal_read` convenience layer composes profiles and the existing BLE
session: names become a checked range, then a normal asynchronous session read.
Dependency direction is convenience layer -> profile + session -> codecs, never
session -> product profile. There is no second connection state machine, queue,
cache or retry policy. Planning failure has no I/O side effect; acceptance is not
completion. Result decoding propagates transport failure before accessing words
and retains the profile's uncertainty flags. The selected profile must remain
consistent between requesting and decoding. Groups use a minimal contiguous
range, not inferred device pages or an automatic all-register sweep.

See [discovery](ble-discovery.md), [session](ble-session.md),
[catalogue](experimental-parameters.md) and [support](support-matrix.md).
