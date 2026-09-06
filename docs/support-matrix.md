# Support matrix

Support claims use four levels:

1. **Detected** — endpoint discovery only.
2. **Transport-compatible** — validated raw register reads.
3. **Profile-compatible** — semantic decoding is available.
4. **Qualified** — model and firmware are checked against a live golden sample.

| Family/profile | RS-485 | BLE | Semantic profile | Qualification |
|---|---|---|---|---|
| SANDISOLAR SD PRO EU 6.5K / AO-6KSL-G3, BB1.0 / ADBA04 / DAAA1600 | Core/example, not physically tested here | Diagnostic protocol reads plus reusable-adapter discovery, named FC04 reads and short direct reconnect tests with Wi-Fi off during BLE | Battery plus owner-compared telemetry in one operating condition | Limited single-unit read qualification, not all fields/counts/firmware or Wi-Fi coexistence |
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
