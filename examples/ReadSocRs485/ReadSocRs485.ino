#include <Arduino.h>
#include <sandisolar.h>

// Change these pins for your ESP32 board and RS-485 transceiver.
constexpr int kRxPin = 16;
constexpr int kTxPin = 17;
constexpr int kDriverEnablePin = 4;
constexpr uint8_t kSlaveId = 1;
constexpr uint32_t kResponseTimeoutMs = 1000;
constexpr uint32_t kPollIntervalMs = 5000;

HardwareSerial inverterSerial(2);

void setTransmitMode(bool enabled) {
  digitalWrite(kDriverEnablePin, enabled ? HIGH : LOW);
  delayMicroseconds(20);
}

bool readExact(uint8_t* output, size_t size, uint32_t timeoutMs) {
  const uint32_t started = millis();
  size_t received = 0;
  while (received < size && millis() - started < timeoutMs) {
    if (inverterSerial.available() > 0) {
      output[received++] = static_cast<uint8_t>(inverterSerial.read());
    } else {
      delay(1);
    }
  }
  return received == size;
}

void setup() {
  Serial.begin(115200);
  pinMode(kDriverEnablePin, OUTPUT);
  setTransmitMode(false);
  inverterSerial.begin(9600, SERIAL_8N1, kRxPin, kTxPin);
  Serial.println("SANDISOLAR read-only RS-485 SOC example");
}

void loop() {
  const auto& profile = sandisolar::aohai_modbus_v214_profile();
  const auto range = sandisolar::signal_read_range(profile, sandisolar::SignalId::BatterySoc);
  // This transport example receives one-word SOC only, not arbitrary groups.
  if (!range || range.value.count != 1) {
    Serial.println("Profile does not provide a one-register SOC read");
    delay(kPollIntervalMs);
    return;
  }
  uint8_t request[sandisolar::kReadRequestSize] = {};
  const auto built = sandisolar::build_read_request(
      kSlaveId, range.value.table, range.value.start, range.value.count, request, sizeof(request));
  if (!built) {
    Serial.printf("Cannot build request: %s\n", sandisolar::error_name(built.error));
    delay(kPollIntervalMs);
    return;
  }

  while (inverterSerial.available() > 0) {
    inverterSerial.read();
  }
  setTransmitMode(true);
  inverterSerial.write(request, built.value);
  inverterSerial.flush();
  setTransmitMode(false);

  // FC04 response for one register: slave, function, count, data[2], CRC[2].
  uint8_t response[7] = {};
  if (!readExact(response, sizeof(response), kResponseTimeoutMs)) {
    Serial.println("Response timeout");
    delay(kPollIntervalMs);
    return;
  }

  uint16_t registers[1] = {};
  const auto parsed = sandisolar::parse_read_response(
      response, sizeof(response), kSlaveId, range.value.table, range.value.count,
      registers, 1);
  if (!parsed) {
    Serial.printf("Invalid response: %s\n", sandisolar::error_name(parsed.error));
    delay(kPollIntervalMs);
    return;
  }

  const sandisolar::RegisterBlockView block = {
      range.value.table, range.value.start, registers, range.value.count};
  const auto soc = sandisolar::decode_signal(
      profile, sandisolar::SignalId::BatterySoc,
      block);
  if (!soc) {
    Serial.printf("Cannot decode SOC: %s\n", sandisolar::error_name(soc.error));
  } else {
    Serial.printf("Battery SOC raw=%lld, interpreted=%.2f %s, quality=%s\n",
                  static_cast<long long>(soc.value.raw),
                  sandisolar::scaled_value(soc.value),
                  sandisolar::unit_symbol(soc.value.unit),
                  soc.value.quality == sandisolar::Quality::Good
                      ? "good"
                      : "unverified_scale");
  }

  delay(kPollIntervalMs);
}
