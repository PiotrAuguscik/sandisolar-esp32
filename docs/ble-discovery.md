# BLE discovery

Include `<sandisolar/arduino_bluedroid_link.h>`. Initialize Bluedroid once, then
use `arduino_bluedroid_scanner()` and one `BleReadSession` from the same task.
Do not initialize another Bluetooth library or mutate this adapter in callbacks.

## API

- `start(seconds, now_ms, optional_exact_name)`: accepts 1..60 seconds; copies
  the optional complete ASCII logger name. Success means accepted, not found.
- `poll(now_ms)`: call regularly; consumes bounded local events, no remote wait.
- `state()`: Idle, Configuring, Starting, Scanning, Complete or terminal Fault.
- `take(result)`: copies one owned result and releases Complete to Idle. A Fault
  result can also be taken once but does not release the terminal fault.
- `select_ble_candidate(result, optional_exact_name)`: requires a successful,
  complete result and exactly one matching candidate. Returns NotFound,
  AmbiguousDevice or IncompleteScan without an arbitrary fallback.
- `candidate.target()`: constructs a target with a name borrowed from that
  candidate. Keep the candidate alive until `session.connect()` returns; connect
  immediately copies the name. Copying a candidate preserves owned name bytes.

See the complete [example](../examples/ReadBatteryBle/ReadBatteryBle.ino).
When several loggers are present, configure their exact advertised name/PN
(e.g. WF12345678), not the inverter serial. RSSI is diagnostic, never selection
authority. A copied target's name is still borrowed; store a candidate instead.

## Discovery limits

Active legacy BLE scanning retrieves names in advertisements or scan responses.
Only complete ASCII local names (AD09) are eligible for the current wire codec;
shortened names (AD08), malformed data and unsupported address types are not
promoted to a usable identity. Each of the two AD sections is bounded to31bytes.

Default candidates have a complete name and either the current **WF + eight
digits** naming hint or an advertised **00ff** service UUID. A supplied exact
name restricts discovery to that name and can admit names outside these hints.
Both 16-bit UUID lists and 128-bit Bluetooth-base-UUID lists are understood.
The service need not be advertised by WF-named loggers; connection subsequently
checks service00ff, characteristicff01 and its properties.

These hints are not exhaustive support for every Aohai product. A name/UUID is
not authentication and cannot prove that a nearby device is your inverter.
There is no generic connect-to-all scan, battery probing or profile inference.

## Completion, ownership and failure

The scanner waits for parameter-set and scan-start callbacks, then natural
inquiry completion. The requested duration is finite; there is no early-cancel
API. Wait for completion and consume the result before opening GATT. GATT
registration/open/read/close all block a new scan; scans block GATT opens.

An independent four-entry control queue preserves lifecycle evidence separately
from a sixteen-entry advertisement queue. The result holds at most16candidates;
duplicates merge by address **and address type**. A changed complete name on
the same endpoint marks the result incomplete. Distinct endpoints with the same
name remain ambiguous. Lost candidate reports/storage exhaustion mark incomplete;
selection rejects such a result even when an exact name was requested.

The serialized completion callback closes advertisement admission. `poll`
drains the bounded candidate queue before publishing Complete. A new scan starts
with an empty result, never the previous device list. Each candidate includes
its last observed RSSI/time; this is not an inverter measurement timestamp.

Parameter/start phases each have a10second deadline; scanning allows the
requested duration plus5seconds. Unsigned millisecond subtraction tolerates a
normal millis wrap when polled regularly. Missing completion, duplicate control
events or control-queue loss cause terminal Fault. Best-effort stop is not a
confirmed recovery barrier; neither scans nor GATT opens may reuse that adapter.
No automatic firmware restart, Wi-Fi change or inverter configuration is made.

Ordinary confirmed setup/start failures produce a failed Complete result; the
caller may take it and decide whether to retry. A synchronous rejection before
scan acceptance returns Error directly and produces no result.

## Backend and qualification

The reference backend is Arduino-ESP32 2.0.17 / ESP-IDF4.4 Bluedroid. Active scan
uses a100ms interval and50ms window. Initialization allocates fixed queues;
candidate parsing/selection uses no heap and callback data is copied immediately.
Regular polling is required to avoid candidate loss. Discovery adds roughly
two KiB of bounded queue/result storage; vendor stack allocation is separate.

The lifecycle follows Espressif's
[GAP API contract](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/bluetooth/esp_gap_ble.html),
including retaining scan parameters through their completion callback. Host
tests exercise the actual adapter with an SDK double; real controller timing,
inverter discovery and Wi-Fi coexistence remain hardware test gates.

Enable the inverter's Bluetooth before first discovery as described in
[README](../README.md#first-connection-enable-bluetooth-on-sandisolar).
