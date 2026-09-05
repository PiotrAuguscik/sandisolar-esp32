#include <Arduino.h>
#include <sandisolar.h>

// OFFLINE decoder demonstration, NOT a device read. Each supplied number is a
// synthetic raw register word, before scaling. For real BLE I/O use
// ReadBatteryBle: read_signal(s)/poll/take/decode_signal, no register arrays.
void printSyntheticReading(sandisolar::SignalId signal, uint16_t rawWord) {
  const auto& profile = sandisolar::aohai_modbus_v214_profile();
  const auto range = sandisolar::signal_read_range(profile, signal);
  if (!range || range.value.count != 1) {
    Serial.println("This fixture helper supports one-word fields only");
    return;
  }
  // Only the test fixture needs a block; addresses come from the same profile.
  const sandisolar::RegisterBlockView block = {
      range.value.table, range.value.start, &rawWord, range.value.count};
  const auto result = sandisolar::decode_signal(profile, signal, block);
  if (!result) {
    Serial.printf("%s: unavailable (%s)\n", sandisolar::signal_name(signal),
                  sandisolar::error_name(result.error));
    return;
  }
  Serial.printf("%s: %.2f %s (synthetic raw=%u, quality=%s)\n",
                sandisolar::signal_name(signal), sandisolar::scaled_value(result.value),
                sandisolar::unit_symbol(result.value.unit), static_cast<unsigned>(rawWord),
                result.value.quality == sandisolar::Quality::Good
                    ? "good" : "unverified_scale");
}

void setup() {
  Serial.begin(115200);
  using sandisolar::SignalId;
  printSyntheticReading(SignalId::BatteryVoltage, 520);          // 52.0 V
  printSyntheticReading(SignalId::BatterySoc, 77);               // candidate 77%
  printSyntheticReading(SignalId::BatteryCurrent, 0xff9c);       // signed -100 -> -1.00 A
  printSyntheticReading(SignalId::BatteryTemperature, 250);     // 25.0 C
  printSyntheticReading(SignalId::BatteryMaxChargeCurrent, 1255); // 125.5 A
  printSyntheticReading(SignalId::BatteryMaxDischargeCurrent, 2400); // 240.0 A
  printSyntheticReading(SignalId::BatteryRatedCapacity, 2800);   // 280.0 Ah
  printSyntheticReading(SignalId::BatterySoh, 98);               // 98%
  // Numeric inverter codes are data, not library/communication errors.
  printSyntheticReading(SignalId::InverterFaultCode, 0);
  printSyntheticReading(SignalId::InverterWarningCode, 103);
  printSyntheticReading(SignalId::InverterFaultSubcode, 0);
  printSyntheticReading(SignalId::InverterWarningSubcode, 0);
}

void loop() {}
