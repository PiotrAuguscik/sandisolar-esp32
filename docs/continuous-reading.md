# Continuous reading and restart recovery

Use the complete [ReadBatteryBle example](../examples/ReadBatteryBle/ReadBatteryBle.ino).
It keeps one `BleReadSession` alive, polls it from one owner and requests battery
measurements every 30 seconds. A successful read returns the session to Ready;
do not disconnect after each measurement. This is ordinary read traffic, not
a command that permanently enables the inverter's Bluetooth.

## Session ownership and reconnects

The session manages framing, validation, deadlines and connection lifecycle.
Your application owns the read cadence and decides when to reconnect. Continue
calling `poll(millis())` while waiting between reads and while closing a link.

After 255 requests, `read_signals()` returns `Error::CorrelationExhausted`.
The example calls `disconnect()`, polls until Idle, then reconnects to the saved
target. Never bypass the fence by wrapping the counter or recreating an object
while its old link is still active. GATT services/handles are rediscovered on
each connection; neither an ESP restart nor another application BLE scan is
needed for this normal renewal. Terminal Fault is not silently retried.

Four such renewals passed in the [10.5-hour single-unit test](support-matrix.md#maintained-session-hardware-check).
This is evidence for short renewal gaps, not an unlimited absence guarantee.

## Save the endpoint, not the connection

The example stores one versioned, CRC-checked NVS blob in namespace `sandi-ble`,
key `logger`, only after the first fully validated response. It contains:

- the six-byte logger address;
- the address type (public or random);
- the full logger name/PN required by the wire codec.

It does not persist pointers, GATT handles, a session epoch or a correlation
counter. The cache is an endpoint hint, not authenticated identity or proof
that the device is currently reachable. No flash write is made per measurement.

On startup a valid record matching the configured `logger_name` is tried
directly. Missing/corrupt/incompatible records lead to normal discovery.
Connection failure does not erase the record or silently select another device;
the example allows three attempts with backoff. A storage failure is printed
and does not prevent read-only use for the current boot.

To deliberately rediscover after replacing a logger, set
`forget_saved_logger_on_boot = true` in the example for a startup, then restore
false. It removes only that example's logger key, not Wi-Fi credentials or
other application NVS data. A changed configured logger name also prevents
the old record from being selected. Random/private addresses may change; do
not assume a cached address remains valid for every logger family.

## Known address versus visibility

An application scan can miss a peripheral because of timing or name filtering,
even when a direct connection attempt would work. However, BLE still requires
the peripheral to send connectable advertising for a new connection. A known
MAC cannot wake a powered-off radio or bypass non-connectable mode. Directed
advertising can also be restricted to a particular peer. See the
[Bluetooth SIG connection procedure](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/architecture%2C-change-history%2C-and-conventions/architecture.html).

The library already attempts a direct connection through `connect(target, now)`;
there is no separate force-connect or wake-by-address operation. If it times out,
enable Bluetooth in the inverter app and retry. Do not claim that enabling it
once guarantees reconnectability after arbitrary downtime.

## Applications with Wi-Fi or uploads

Keep blocking HTTPS/storage/UI work away from the session owner. A practical
application uses one BLE-owner task and a bounded queue of owned results for
a separate upload task. That task must not call the session or scanner. Wi-Fi
loss alone need not stop local sampling, but queue/storage limits and recovery
policy belong to the application; never grow an unbounded JSON history in RAM.

The hardware test used this split, a 30-second cadence, small upload batches
and a memory safety guard. It does not qualify arbitrary TLS buffers, task
stacks or high-rate polling. Configure a compatible Wi-Fi power-save mode
before BLE startup, do not change it while BLE runs, and retain a remote
recovery path. See [Wi-Fi preflight](../README.md#applications-that-also-use-wi-fi).

## Recovery tests are distinct

| Case | What it establishes |
|---|---|
| Short correlation-limit renewal | A fresh session can read the same target immediately after clean closure; hardware-tested |
| Long clean disconnection followed by an ESP restart | Direct connection failed after about 39 minutes on the tested unit; three subsequent direct retries also timed out without Enable/Set |
| Unexpected ESP power loss | Recovery after an unannounced link loss; not equivalent to clean closure |
| Logger/inverter power cycle or address change | Endpoint invalidation and rediscovery; separate from an ESP-only restart |

For a long-gap test, first confirm clean session closure, record the time,
leave BLE unused for the chosen duration, then attempt the known address/type/
name without scanning. Require validated telemetry, not just GATT connection.
If it fails, record the error before using Enable/Set or rediscovery. Keep
normal inverter operation and the remote ESP's working Wi-Fi unchanged.

In the single-unit inactivity test, the app still displayed Enabled, but a
30-second direct connection attempt failed after about 39 minutes without BLE.
Five subsequent 60-second discovery scans found no matching logger; three more
direct attempts spaced about three minutes apart also timed out. Wi-Fi stayed
available. Treat Enable/Set as the recovery step when Bluetooth becomes
unavailable; the exact internal disable timer/state is not established.
