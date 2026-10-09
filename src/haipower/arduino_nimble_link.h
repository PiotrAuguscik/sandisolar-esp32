#pragma once
#include "haipower/ble_session.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(SANDISOLAR_USE_NIMBLE)
namespace haipower {
// Application owns NimBLE initialization and radio policy. Initialize NimBLE
// once, set preferred MTU >= 285, then attach. Never deinit while clients live.
// Requires NimBLE-Arduino 2.3.6. No scan or global controller reconfiguration.
// Owns one process-lifetime client and bounded worker/queues. One application
// task owns the session; synchronous GATT calls run on the adapter worker.
Error attach_arduino_nimble();
BleLink& arduino_nimble_link();
}  // namespace haipower
#endif
