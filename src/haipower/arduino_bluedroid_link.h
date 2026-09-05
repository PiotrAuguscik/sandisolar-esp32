#pragma once

#include "haipower/ble_session.h"
#include "haipower/ble_discovery.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_idf_version.h>
#if ESP_IDF_VERSION_MAJOR == 4
namespace haipower {
// Reference adapter: Arduino-ESP32 2.x / ESP-IDF 4.4 Bluedroid, one logger.
// Call once in setup BEFORE any other BLE library. Initialization can block;
// connect/read/poll do not wait for remote events. Refuses an existing BT stack.
// Owns BLE callbacks/controller for the process lifetime, never Wi-Fi or an
// application NVS namespace. The SDK can maintain its own Bluetooth cache.
Error initialize_arduino_bluedroid();
BleLink& arduino_bluedroid_link();
// Same controller owner as the GATT link; scan and GATT are mutually exclusive.
BleScanner& arduino_bluedroid_scanner();
}  // namespace haipower
#endif
#endif
