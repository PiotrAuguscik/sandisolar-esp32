# BLE read session

Include `<sandisolar/arduino_bluedroid_link.h>` for the experimental
Arduino-ESP32 2.x / ESP-IDF 4.4 Bluedroid adapter. Portable `BleReadSession`
composes the validated codec with the `BleLink` lifecycle contract.

## API and ownership

`initialize_arduino_bluedroid()` is a one-time, potentially blocking setup call.
It refuses a previously initialized Bluetooth stack. A process-lifetime adapter
owns the controller, GAP/GATT callbacks and bounded queues. Do not initialize
BLEDevice/NimBLE or replace those callbacks. Application Wi-Fi is left untouched.

Use one session and one owner task; crypto/link dependencies must outlive the
session. Call `poll(millis())` regularly. Keep the session alive until Idle or
Fault; destruction is not an asynchronous close. Do not recreate a session to
bypass an unresolved terminal Fault.

- `connect(target, now_ms)` accepts MAC, public/random address type and complete
  logger name. [Discovery](ble-discovery.md) supplies these; the name is copied
  immediately. Success means accepted, not Ready.
- `read({slave, table, start, count}, now_ms)` accepts FC03/FC04 only,
  slave 1..247, count 1..125 and a valid register range. Success means accepted;
  even a subsequent write-submission error is delivered through `take`.
- `poll(now_ms)` advances the state without waiting for remote completion.
- `take(result)` copies one owned terminal read result. Only `Error::None`
  makes registers usable. Consume it before another read/connection.
- `disconnect(now_ms)` initiates close; continue polling for the barrier.
- `state()`, `last_error()` and `last_packet_error()` expose diagnostics.

Rejected operations produce no result. Accepted reads produce one terminal
result; connect failures use state/last_error instead. A result owns up to 125
registers, request metadata, connection epoch, request time and callback arrival
time. Arrival is not the inverter's sample time. Copied results are not mutated
by later reads and must not be presented as newly sampled.

The application explicitly chooses a profile. Decode successful registers using
a borrowed `RegisterBlockView` whose storage remains alive. There is no model
auto-resolution, freshness policy, implicit read retry or multi-page atomic
snapshot. Missing or invalid values are not replaced with zero.

## Lifecycle and failure

```text
Idle -> Connecting -> Ready -> Reading -> Ready
          |            |        |
          +------------+--------+-> Closing -> Idle (confirmed barrier)
                                      |
                                      +-> Fault (missing/lost lifecycle evidence)
```

- Exactly one read is in flight. A fully validated frame AND successful ATT
  write completion are required. Notification-before-ACK waits for ACK.
- Wrong identity/correlation, CRC, length, marker or zero padding is diagnostic
  only. No valid reply before the deadline produces Timeout and closes the link.
- Connect deadline 30 seconds, read 10 seconds, close 10 seconds. Deadlines are
  checked before queued events. Unsigned monotonic millisecond subtraction
  tolerates a normal wrap when polled regularly, not a whole wrap with no polling.
- A one-second minimum gap after submission/completion survives reconnect.
  Applications may choose a slower cadence; the example uses 30 seconds.
- Correlations 0..254 are used once per connection. CorrelationExhausted requires
  close/reconnect. Epochs fence local queued callbacks; they are not protection
  against an adversarial peripheral replay.
- Close retires the GATT application and waits for UNREG. Outstanding register/
  open operations must complete before cleanup. Missing completion, lost events
  or uncertain close becomes terminal Fault, never optimistic Idle.
- No automatic reboot, radio reset, Wi-Fi management or inverter configuration.
  Fault has no library reset API. The application owns any later recovery.

Scan and GATT are mutually exclusive. Consume a completed scan before connect.
A scanner terminal fault also fences new GATT opens on the shared controller.

## GATT and framing

The adapter requires exactly one service 00ff and one characteristic ff01 with
write-with-response and notify. It resolves CCCD 2902, writes 01 00 and waits
for descriptor-write success before Ready. Service changes invalidate the link.
Close/unregister completes before a new connection discovers handles again.

The wire format uses zero-filled AES128-CBC with logger identity and nested
Modbus RTU. Complete-frame validation checks both envelope lengths,
protocol/marker, correlation, outer big-endian CRC, logger identity, inner
RTU shape/CRC and an all-zero tail. Fixed protocol AES material is obfuscation,
not authentication. `parse_ble_read_frame` is the complete validator;
diagnostic `decrypt_ble_notification` alone does not validate telemetry.

One complete frame must fit one notification: request AND expected reply must
fit `MTU - 3`, or `read` rejects MtuTooSmall before I/O. No speculative fragment
reassembly. A ten-character logger/count 125 needs 282 notification bytes
(MTU >=285); count 1 needs 42 bytes (MTU >=45). The adapter requests MTU 512.

## Resources and qualification

The portable session uses no heap. The adapter allocates an eight-entry GATT
queue (~4.4 KiB) plus separate bounded discovery queues at initialization.
Callbacks copy data and perform no user callback, logging, crypto or remote
wait. Notification overflow/oversize is terminal. Each session poll handles at
most eight port events; native SDK enqueue/cache operations can take internal
locks. This is cooperative I/O, not a hard-real-time latency guarantee.

Tests exercise the actual adapter using a bounded SDK double and compile the
public example with real ESP headers/libraries. Host tests alone do not qualify
controller timing or power-loss recovery. A separate 10.5-hour single-unit run
validated sustained reads, four correlation-limit reconnects and concurrent
Wi-Fi/HTTPS on the tested C3/backend. See [support](support-matrix.md) for the
scope and [continuous reading](continuous-reading.md) for application ownership,
saved targets and cases that still require testing.
