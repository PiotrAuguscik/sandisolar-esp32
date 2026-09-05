#include <sandisolar.h>
#include <sandisolar/arduino_bluedroid_link.h>

// Leave empty to select the only matching logger, or enter YOUR logger name
// (e.g. WF12345678, NOT the inverter serial) to disambiguate several devices.
// No MAC address is required. This is a discovery hint, not authentication.
const char logger_name[] = "";
// Explicit profile choice, NOT automatic model qualification. Select what you
// want to read by name; the library derives addresses, widths and scaling.
const auto& profile = sandisolar::aohai_modbus_v214_profile();
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
auto& scanner = sandisolar::arduino_bluedroid_scanner();
sandisolar::BleScanCandidate selected{};
sandisolar::BleScanResult scan_result{}; // static, not the Arduino loop stack
bool have_target = false;
sandisolar::BleReadSession session(sandisolar::arduino_bluedroid_link(),
    sandisolar::esp32_mbedtls_aes128_cbc());
bool enabled = false;
bool waiting = false;
unsigned attempts = 0;
uint32_t action_at = 0;
uint32_t last_read_at = 0;
bool have_read = false;

void setup() {
  Serial.begin(115200);
  const auto error = sandisolar::initialize_arduino_bluedroid();
  enabled = error == sandisolar::Error::None;
  Serial.println(sandisolar::error_name(error));
  Serial.println("Before first discovery: HaiPower app -> General Setting -> Bluetooth Enabled -> Enable -> Set.");
  Serial.println("Keep a phone's Local Tool disconnected while the ESP connects.");
  // Application may configure Wi-Fi independently. This library does not.
}

void loop() {
  const uint32_t now = millis();
  scanner.poll(now);
  session.poll(now);  // keep draining close even when new operations are disabled
  if (!enabled) { delay(1); return; }
  if (scanner.take(scan_result)) {
    for (size_t i = 0; i < scan_result.count; ++i) {
      const auto& candidate = scan_result.candidates[i];
      Serial.printf("Candidate %s %02X:%02X:%02X:%02X:%02X:%02X type=%u RSSI=%d\n",
          candidate.advertisement.name, candidate.address[0], candidate.address[1],
          candidate.address[2], candidate.address[3], candidate.address[4], candidate.address[5],
          static_cast<unsigned>(candidate.address_type), candidate.rssi);
    }
    auto choice = sandisolar::select_ble_candidate(scan_result,
        {logger_name, sizeof(logger_name) - 1});
    if (choice) {
      selected = choice.value; // owned name remains alive for target()/connect()
      have_target = true;
      attempts = 0;
    } else {
      Serial.println(sandisolar::error_name(choice.error));
      if (choice.error != sandisolar::Error::NotFound) {
        Serial.println("No automatic fallback. Check the logger name / scan error.");
        enabled = false;
      }
    }
  }
  if (!enabled) { delay(1); return; }
  if (!have_target) {
    if (scanner.state() == sandisolar::BleScanState::Idle &&
        attempts < 3 && (attempts == 0 || now-action_at >= 30000)) {
      ++attempts;
      action_at = now;
      const auto error = scanner.start(15, now, {logger_name, sizeof(logger_name) - 1});
      Serial.println(sandisolar::error_name(error));
      if (error != sandisolar::Error::None) enabled = false;
    }
    delay(1);
    return;
  }
  sandisolar::BleReadResult result{};
  if (session.take(result)) {
    waiting = false;
    have_read = true;
    last_read_at = now;
    if (result.error != sandisolar::Error::None) {
      Serial.println(sandisolar::error_name(result.error));
      // No zero-valued fallback and no stale cache masquerading as a new read.
    } else {
      attempts = 0; // reset failure budget only after validated telemetry
      for (auto signal : signals) {
        auto reading = sandisolar::decode_signal(profile,signal,result);
        if (reading) {
          Serial.printf("%s=%.2f%s raw=%lld age_ms=%lu quality=%s\n",
              sandisolar::signal_name(signal),sandisolar::scaled_value(reading.value),
              sandisolar::unit_symbol(reading.value.unit),
              static_cast<long long>(reading.value.raw),
              static_cast<unsigned long>(now-result.received_at_ms),
              reading.value.quality == sandisolar::Quality::Good ? "good" : "unverified_scale");
        } else {
          Serial.printf("%s: unavailable (%s)\n", sandisolar::signal_name(signal),
              sandisolar::error_name(reading.error));
        }
      }
    }
  }
  if (session.state() == sandisolar::BleSessionState::Fault) {
    Serial.println("BLE lifecycle uncertain; halted, no automatic reboot.");
    enabled = false;
  } else if (session.state() == sandisolar::BleSessionState::Idle) {
    // At most three consecutive attempts, 30 seconds apart. Bluetooth may
    // require manual Enable/Set in the inverter app. No configuration writes.
    if (attempts < 3 && (attempts == 0 || now-action_at >= 30000)) {
      ++attempts;
      action_at = now;
      const auto error = session.connect(selected.target(),now);
      Serial.println(sandisolar::error_name(error));
    }
  } else if (session.state() == sandisolar::BleSessionState::Ready && !waiting &&
             (!have_read || now-last_read_at >= 10000)) {
    const auto error = sandisolar::read_signals(session,profile,signals,now);
    if (error == sandisolar::Error::None) waiting = true;
    else if (error == sandisolar::Error::CorrelationExhausted) {
      // Epoch fence before wire correlation is reused. No ESP restart.
      session.disconnect(now);
    } else if (error != sandisolar::Error::Busy) {
      Serial.println(sandisolar::error_name(error));
      session.disconnect(now);
      enabled = false;
    }
  }
  delay(1);  // application yields; session.poll itself does not wait
}
