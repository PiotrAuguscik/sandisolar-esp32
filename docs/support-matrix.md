# Support matrix

Support claims use four levels:

1. **Detected** — endpoint discovery only.
2. **Transport-compatible** — validated raw register reads.
3. **Profile-compatible** — semantic decoding is available.
4. **Qualified** — model and firmware are checked against a live golden sample.

| Family/profile | RS-485 | BLE | Semantic profile | Qualification |
|---|---|---|---|---|
| SANDISOLAR SD PRO EU 6.5K / AO-6KSL-G3, BB1.0 / ADBA04 / DAAA1600 | Core/example, not physically tested here | Discovery, named FC04 reads, 10.5-hour maintained-session run with concurrent Wi-Fi/HTTPS and four correlation-limit reconnects | Battery plus owner-compared telemetry; observed SOC 97..100 retains its scale-quality flag | Single-unit C3/backend qualification, not all fields/counts/firmware, routers or power-loss recovery |
| Generic Aohai Modbus V2.14-1 | Implemented core and example | Codec available, transport qualification per device | Implemented, experimental | No universal qualification |
| Other AO/AH/FSA/FSC/HSA/HUA families | Unknown | Static app evidence only | No automatic fallback | Not qualified |
| Other batteries, EV chargers and logger families | Unknown | Unknown | None | Not qualified |

Finding a device or successfully reading one register does not promote it to a
profile-compatible or qualified status.

The optional [experimental catalogue](experimental-parameters.md) covers
297 numeric register interpretations with evidence/applicability metadata.
It is broader map coverage, not additional model or physical-sensor qualification.

Validated protocol ranges on that one unit: FC04 starts 0/125/250/375, count125;
selected short FC04 at128/count1 and141/count5; FC03 at2/count1,
5/count36 and50/count15. This is protocol interoperability evidence, not an
endurance test of the reusable adapter. Other legal reads remain unqualified.

Discovery recognizes complete WF-plus-eight-digit names and named00ff
advertisers, with an optional exact-name filter. It does not auto-identify an
inverter model. On one ESP32-C3 and one inverter, the reusable adapter passed
exact-name discovery, five groups covering all 30 named signals, four direct
connections with three scheduled reconnects, and 197 successful sequential SOC
reads within a 330-second test. The latter ended at its test deadline; it did
not reach correlation rollover. These tests used Wi-Fi off during BLE and do
not establish long endurance, scale correctness across operating conditions,
or reliable concurrent Wi-Fi/BLE operation. The Wi-Fi-on attempt was interrupted;
the adapter now rejects the known conflicting power-save configuration before
Bluetooth startup. See [discovery](ble-discovery.md).

## Maintained-session hardware check

On 2026-09-07 the same reusable session/adapter (revision d4dda89) completed
1254/1254 validated FC04 reads over approximately 10.5 hours on one ESP32-C3,
Arduino-ESP32 2.0.17 / ESP-IDF 4.4. An application-owned BLE task polled the
session while another task uploaded bounded telemetry batches over HTTPS.
Wi-Fi remained enabled with modem sleep; the library did not configure it.

- Five named groups (battery, power, PV/grid, energy, status), 30 s idle cadence.
- Four successful close/reconnect cycles at the 255-request correlation fence,
  using the known target without another application discovery scan.
- No ESP restart during the run, no observed Wi-Fi disconnect, no lost samples
  or missing sample/batch sequence numbers; clean operator-requested stop.
- Minimum observed application free heap 31596 B; the application guard was
  24576 B. These are that firmware's measurements, not a library RAM guarantee.
- SOC raw values varied from 100 down to 97. The owner compared both 100% and
  97% with the app on this unit. Around 20:40 local time the app also matched
  SOH 100%, battery 52.3 V, -1.9 A and 17.7 degrees C from the evening reads.
  Cloud reporting is not synchronized with each BLE sample. Other SOC ranges
  and models remain unqualified; the scale-quality flag stays conservative.

This is not proof of unbounded uptime, all Wi-Fi/TLS workloads, sudden power-loss
recovery, other models or persistent visibility after Bluetooth is disabled.
The updated NVS example has host record-validation tests and real-header C3
compilation; its flash power-loss behavior was not physically tested in this run.

After clean closure and about 39 minutes without BLE, direct connection to the
saved address timed out. Five 60-second exact-name scans and three subsequent
direct attempts also failed without another Enable/Set. The cloud UI still
showed Enabled. Thus short session renewal is tested, but unattended recovery
after long inactivity is not supported by this experiment; see
[restart recovery](continuous-reading.md).
