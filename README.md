# SANDISOLAR ESP32

An ESP32 library for reading measurements, status and parameters from
**[SANDISOLAR inverters](https://www.sandisolar.com/product/sd-pro-6-5-kw-off-grid-inverter-with-ai-assistant/)
over Bluetooth** — including battery state of charge, voltages, currents,
solar power, load, energy counters and fault codes.

The library discovers the inverter's Bluetooth logger, connects to it and reads
data locally. No cloud account is needed for these reads. It provides decoded
values and raw registers; it does not change inverter settings.

<p>
  <a href="https://github.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration/blob/224306a41fafe6603dbbb38728768a19de0a8024/SD-PRO-EU-6_8.webp">
    <img src="https://raw.githubusercontent.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration/224306a41fafe6603dbbb38728768a19de0a8024/SD-PRO-EU-6_8.webp" alt="SANDISOLAR SD PRO EU inverter, shown in the upstream AO-6KSL-G3 integration" width="300">
  </a>
</p>

Product image: [upstream integration](https://github.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration).
Externally referenced for identification; the image is not included in this
repository's MIT license. Appearance alone does not establish compatibility.

> [!WARNING]
> Independent, experimental project; not affiliated with SANDISOLAR or Aohai.
> Validated protocol reads and some owner-compared values come from one
> SD PRO EU 6.5K / AO-6KSL-G3. The reusable adapter has passed single-unit
> discovery, named reads and short reconnect tests with Wi-Fi off during BLE.
> Long endurance and Wi-Fi coexistence remain unqualified. The library has
> not been released yet. Parameter coverage depends on the model and firmware;
> some interpretations are explicitly experimental.

## What is included

- BLE logger discovery, explicit/unique-device selection, connection and reads.
- Encrypted BLE framing, identity/correlation checks and both CRCs validated.
- Reads by parameter name, individually or in a group; no register addresses
  or manual scale calculations in normal BLE application code.
- 30 named telemetry signals: battery, PV, grid, load, EPS, energy and numeric
  inverter fault/warning codes. Code descriptions belong to the model manual.
- An opt-in catalogue of **297 register interpretations in total**, including
  those 30. Uncertain meaning, encoding and scale are marked separately.
- Owned raw register words for custom interpretations.
- Portable C++11 Modbus RTU codecs, an RS-485 example, tests and API documentation.

No register-write, inverter configuration, firmware-upgrade, cloud-login or
MQTT API. Your application owns Wi-Fi, polling cadence, caching and automation.

## First connection: enable Bluetooth on SANDISOLAR

On the tested inverter, Bluetooth discovery must first be enabled in the
**[HaiPower app for Android](https://play.google.com/store/apps/details?id=com.aohai.power)**,
in the inverter's **Setting → General Setting** screen:

1. Set **Bluetooth Enabled → Enable**.
2. Press **Set**; selecting Enable alone is not sufficient.
3. Start the ESP's BLE scan promptly, while the inverter/logger is visible.

After discovery, reconnect directly using the saved logger address, address
type and name; another scan is not required while those details remain valid
and the logger is connectable. The example keeps the selected logger in memory.
To reuse it after restarting the ESP, your application must persist those details.
If the logger is unavailable, repeat **Enable → Set** and scan again. The library
does not enable Bluetooth or change inverter settings itself.

The ESP must be near the inverter/logger. Avoid a simultaneous phone connection
using **Local Tool** while the ESP connects. An ordinary cloud view in the app
is not the same as a local Bluetooth connection.

## Install and run

The concrete BLE adapter currently targets **Arduino-ESP32 2.0.17 / ESP-IDF 4.4**
(tested compilation: ESP32-C3, PlatformIO Espressif32 6.8.1). It is not yet an
Arduino 3.x, NimBLE or standalone ESP-IDF BLE adapter. Portable codecs/decoders
do not have this BLE backend restriction. See [support](docs/support-matrix.md).

For Arduino IDE, install the repository as a library and select the compatible
ESP32 core. For a local PlatformIO checkout next to your application:

```ini
[env:esp32c3]
platform = espressif32@6.8.1
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
lib_deps = symlink://../sandisolar-esp32
```

Use [ReadBatteryBle](examples/ReadBatteryBle/ReadBatteryBle.ino):

- No BLE MAC address is needed. Leave `logger_name` empty to select the only
  candidate, or set your **logger name/PN**, e.g. `WF12345678`, if several exist.
  This is not the inverter serial number. Examples contain synthetic values.
- The sketch scans for 15 seconds, lists candidates, connects, and reads battery
  SOC, SOH, voltage, current, temperature, current limits and capacity every ten
  seconds. Edit its `signals` array to choose parameters. It bounds retry attempts.
- Several matching devices, incomplete scan results and missing data are errors;
  it never selects the strongest signal or replaces failed measurements with 0.

Public includes use `<sandisolar.h>` and
`<sandisolar/arduino_bluedroid_link.h>`, with namespace `sandisolar`.

### Applications that also use Wi-Fi

On this IDF4 backend, starting Bluetooth with Wi-Fi modem sleep disabled
(`WiFi.setSleep(false)` / `WIFI_PS_NONE`) can abort inside the vendor coexistence
code. `initialize_arduino_bluedroid()` therefore returns **`Error::RadioConflict`**
(`radio_conflict`) before allocating queues or starting Bluetooth when it detects
that configuration. It does not disconnect, reconfigure or turn off Wi-Fi.

Your application owns radio configuration. A rejected preflight can be retried
after the application selects a compatible configuration; do not disable modem
sleep while BLE is active. Passing this check does not guarantee reliable
Wi-Fi/BLE coexistence on a particular board/router. Keep a remote device's working
network configuration unchanged until an alternative has been qualified.
See [backend limits and tests](docs/ble-discovery.md#backend-and-qualification).

## API at a glance

```cpp
// One-time setup. Check every returned Error before proceeding.
sandisolar::initialize_arduino_bluedroid();
auto& scanner = sandisolar::arduino_bluedroid_scanner();
scanner.start(15, millis());       // optionally pass exact logger name/PN

// In loop(): scanner.poll(millis()); when scanner.take(scanResult) succeeds:
auto candidate = sandisolar::select_ble_candidate(scanResult);
if (candidate) {
  session.connect(candidate.value.target(), millis()); // name copied now
}
// session.poll(millis()); after state()==Ready:
const auto& profile = sandisolar::aohai_modbus_v214_profile();
sandisolar::read_signal(session, profile, sandisolar::SignalId::BatterySoc, millis());
// poll + take(readResult); registers are usable only when error == Error::None.
```

This outline omits declarations and state handling; use the complete example
above. The adapter owns the Bluetooth stack exclusively. Do not initialize
BLEDevice/NimBLE alongside it. Scan and GATT connection are mutually exclusive;
consume scan results before connecting. The application explicitly chooses the
inverter profile. Discovery hints are not authenticated identity or model proof.

- [BLE discovery and selection](docs/ble-discovery.md)
- [BLE session, lifecycle and errors](docs/ble-session.md)
- [Experimental parameters and complete catalogue](docs/experimental-parameters.md)
- [Architecture and responsibility boundaries](docs/architecture.md)

For wider parameters include `<sandisolar/experimental.h>`. Retain the proposed
interpretation's `ProtocolMap`, `Hypothesis`, `ConflictingSources` or `Unknown`
metadata and original words. **297 interpretations does not mean 297 working
sensors.** SOC register 128 retains a conservative scale-quality flag: raw 100
matched 100% on the tested unit, but other ranges/models still need comparison.
Timestamps describe callback arrival, not the inverter's acquisition time.

## Read SOC — no register numbers

The following fragments run **after discovery and connection**, using the
`session` from [ReadBatteryBle](examples/ReadBatteryBle/ReadBatteryBle.ino).
Request a read once when ready; keep calling `session.poll(millis())` in `loop()`.
Use the same profile to request and decode a result.

```cpp
const auto& profile = sandisolar::aohai_modbus_v214_profile();
const auto error = sandisolar::read_signal(
    session, profile, sandisolar::SignalId::BatterySoc, millis());
if (error != sandisolar::Error::None) {
  Serial.println(sandisolar::error_name(error));
}
```

`Error::None` means **request accepted**, not that a value is already available.
Later, in `loop()`:

```cpp
session.poll(millis());
sandisolar::BleReadResult result{};
if (session.take(result)) {
  const auto soc = sandisolar::decode_signal(
      sandisolar::aohai_modbus_v214_profile(), sandisolar::SignalId::BatterySoc, result);
  if (soc) {
    Serial.printf("SOC: %.1f %% (quality=%s)\n",
        sandisolar::scaled_value(soc.value),
        soc.value.quality == sandisolar::Quality::Good ? "good" : "unverified_scale");
  } else {
    Serial.printf("SOC unavailable: %s\n", sandisolar::error_name(soc.error));
  }
}
```

The result overload propagates read failures. It does not convert a timeout to
0% or silently use an earlier measurement. Decoding handles signedness;
`scaled_value()` applies the profile's scale. Keep `Reading::quality` alongside the displayed value.
Decoding performs no I/O and can be repeated on the same owned result.

## Read several parameters together

These are alternative **global `signals` arrays** for the complete
[ReadBatteryBle sketch](examples/ReadBatteryBle/ReadBatteryBle.ino). Replace its
array with one below; its existing loop handles the connection, read, errors and
printing. No addresses, offsets, register widths or scale factors are needed.

Battery measurements (the default):

```cpp
const sandisolar::SignalId signals[] = {
    sandisolar::SignalId::BatterySoc,
    sandisolar::SignalId::BatterySoh,
    sandisolar::SignalId::BatteryVoltage,
    sandisolar::SignalId::BatteryCurrent,
    sandisolar::SignalId::BatteryTemperature,
    sandisolar::SignalId::BatteryMaxChargeCurrent,
    sandisolar::SignalId::BatteryMaxDischargeCurrent,
    sandisolar::SignalId::BatteryRatedCapacity,
};
```

Power readings — all returned in watts, including separate battery charging and
discharging values:

```cpp
const sandisolar::SignalId signals[] = {
    sandisolar::SignalId::PvTotalPower,
    sandisolar::SignalId::ActiveOutputPower,
    sandisolar::SignalId::PowerToUser,
    sandisolar::SignalId::GridExportPower,
    sandisolar::SignalId::LoadPower,
    sandisolar::SignalId::EpsActivePowerR,
    sandisolar::SignalId::BatteryChargePower,
    sandisolar::SignalId::BatteryDischargePower,
};
```

PV, grid and EPS electrical readings (V, A and Hz):

```cpp
const sandisolar::SignalId signals[] = {
    sandisolar::SignalId::Pv1Voltage,
    sandisolar::SignalId::Pv1Current,
    sandisolar::SignalId::GridVoltageR,
    sandisolar::SignalId::GridCurrentR,
    sandisolar::SignalId::GridFrequency,
    sandisolar::SignalId::EpsVoltageR,
    sandisolar::SignalId::EpsFrequency,
};
```

Energy counters (kWh):

```cpp
const sandisolar::SignalId signals[] = {
    sandisolar::SignalId::PvEnergyToday,
    sandisolar::SignalId::PvEnergyTotal,
};
```

Inverter status and numeric fault/warning codes, without a description dictionary:

```cpp
const sandisolar::SignalId signals[] = {
    sandisolar::SignalId::InverterStatus,
    sandisolar::SignalId::InverterFaultCode,
    sandisolar::SignalId::InverterWarningCode,
    sandisolar::SignalId::InverterFaultSubcode,
    sandisolar::SignalId::InverterWarningSubcode,
};
```

Each group uses the same request call, only when the session is ready and the
previous result has been consumed:

```cpp
const auto error = sandisolar::read_signals(session, profile, signals, millis());
if (error != sandisolar::Error::None) {
  Serial.println(sandisolar::error_name(error));
}
```

For every successful `take(result)`, decode all members of the requested group:

```cpp
for (auto signal : signals) {
  const auto reading = sandisolar::decode_signal(profile, signal, result);
  if (!reading) {
    Serial.printf("%s: unavailable (%s)\n", sandisolar::signal_name(signal),
        sandisolar::error_name(reading.error));
    continue;
  }
  Serial.printf("%s: %.2f %s (raw=%lld, quality=%s)\n",
      sandisolar::signal_name(signal), sandisolar::scaled_value(reading.value),
      sandisolar::unit_symbol(reading.value.unit),
      static_cast<long long>(reading.value.raw),
      reading.value.quality == sandisolar::Quality::Good ? "good" : "unverified_scale");
}
```

For a numeric code, use `reading.value.raw`; a communication error is instead
`reading.error`. `Quality::Good` is profile/decoding metadata, **not** a guarantee
that a parameter is physically supported by every inverter or that data is fresh.

### Group limits and advanced use

The library derives the smallest contiguous range covering a group, including
both words of 32-bit values and any intervening registers. All signals must use
the same table and fit in **125 registers**; otherwise `FieldOutOfRange` is
returned **before sending anything**. Unknown signals return `UnsupportedSignal`.
An empty group is invalid. No partial group is silently read.

Read the groups above separately to cover all 30 named signals. Do not concatenate
them into one request: e.g. SOC and total PV energy are too far apart. Request
one group, consume its result, then request the next at your chosen cadence.
`Busy` can also mean the session's minimum one-second command gap has not elapsed.
Results from different reads are not an atomic snapshot. Compact group ranges
are covered by software tests; hardware coverage is limited to full pages and
selected short reads.

For raw RTU/RS-485 or custom transport code, `signal_read_range(profile, signal)`
(or the array overload) exposes the derived table/start/count without doing I/O.
The lower-level `session.read(...)`, `RegisterBlockView` and raw decoders remain
available when you deliberately need register-level access. The opt-in
experimental catalogue still exposes addresses as stable identifiers because
some parameter meanings are not yet known reliably.

`DecodeCapturedPage` is only a synthetic decoder demonstration: its raw sample
numbers represent fake input data, not values an application must supply to read
an inverter. Normal BLE code uses the named APIs above and receives actual
registers from the session.

## Other examples

- [DecodeCapturedPage](examples/DecodeCapturedPage/DecodeCapturedPage.ino):
  decode synthetic battery data and numeric fault/warning codes offline.
- [DecodeExperimentalPage](examples/DecodeExperimentalPage/DecodeExperimentalPage.ino):
  inspect uncertainty and apply a custom interpretation to original words.
- [EncodeBlePageRead](examples/EncodeBlePageRead/EncodeBlePageRead.ino):
  construct an encrypted read request offline.
- [ReadSocRs485](examples/ReadSocRs485/ReadSocRs485.ino): a direct RS-485 example;
  verify your inverter's user/Modbus port and pinout. A suitable 3.3 V-compatible
  transceiver is required; never connect ESP GPIO directly to RS-485 A/B.

## Tests and scope

Host tests require a C++11 compiler, CMake >=3.16 and OpenSSL development
headers/library. OpenSSL is a host-test dependency only, not an ESP dependency.

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tools -p 'test_*.py'
```

Tests cover named read planning, codecs, decoders, malformed responses, discovery selection and BLE
lifecycle failure transitions. An SDK double tests actual adapter code, but
cannot prove real radio/RTOS behavior. Automated ESP32-C3 compilation checks the
public example without flashing. Hardware qualification is a separate step.

The library rejects invalid/mismatched frames. Applications must also reject
stale or insufficiently qualified values before using them for automation.
Multi-page atomic snapshots and automatic model/profile resolution are not
implemented. Fault/warning numbers are inverter data; `Error` describes a
library/communication operation and must not be confused with those numbers.

## Protocol sources and attribution

Protocol mapping benefited from
[bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration](https://github.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration)
and its published register map. Thank you to its author. This is a
separate C++ implementation, not a fork or bundled copy of that integration.

- **App used for protocol analysis:** [HaiPower on Google Play](https://play.google.com/store/apps/details?id=com.aohai.power),
  Android package `com.aohai.power`. Local BLE framing was reconstructed from
  version **2.0.3**. The store link may now offer a newer version; it is not a
  pinned download of the analyzed APK.
- **Register addresses, types and scales:** [Modbus RTU Protocol V2.14-1 register-map archive](https://github.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration/blob/224306a41fafe6603dbbb38728768a19de0a8024/Modbus%20RTU%20Protocol%20V2.14-1.zip),
  containing the `.xlsx` workbook used for the profile and experimental catalogue.
  This is the inverter-specific map, not just the generic Modbus specification.
  It documents register interpretations; the BLE encryption/envelope is separate.

The register-map and image links are pinned to an upstream revision. Those
third-party materials retain their own rights; this project's MIT license
does not relicense them.

This repository contains the library, examples, tests and user/API documentation.
APK/decompiled code, vendor archives, capture tools, field-laboratory firmware,
private captures and research notebooks are not distribution dependencies.

## AI-assisted development

This library was developed with **AI assistance**, including protocol analysis,
C++ code, tests, examples and documentation. AI-generated work can contain errors.
Automated tests and the limited device comparisons described above do not replace
independent review or hardware qualification. Uncertain parameters remain marked;
do not treat this library as safety-certified inverter control software.

## Contributing and license

See [CONTRIBUTING.md](CONTRIBUTING.md). MIT: [LICENSE](LICENSE).
