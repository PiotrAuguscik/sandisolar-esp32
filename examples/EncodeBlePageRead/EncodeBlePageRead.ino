#include <Arduino.h>
#include <sandisolar.h>

void setup() {
  Serial.begin(115200);
  delay(500);

  const sandisolar::Aes128Cbc crypto = sandisolar::esp32_mbedtls_aes128_cbc();
  uint8_t frame[sandisolar::kBlePageReadFrameSize] = {};
  const auto result = sandisolar::build_ble_page_read(
      {"WF12345678", 10},  // Replace with the selected logger's BLE name.
      1, sandisolar::RegisterTable::Holding, 0, 125, 0x42, crypto, frame,
      sizeof(frame));
  if (!result) {
    Serial.printf("BLE frame error: %s\n", sandisolar::error_name(result.error));
    return;
  }

  Serial.println("HaiPower BLE page-read frame:");
  for (size_t index = 0; index < result.value; ++index) {
    Serial.printf("%02x%s", frame[index], index + 1 == result.value ? "\n" : " ");
  }
}

void loop() {}
