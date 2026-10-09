#include <Arduino.h>
#include <NimBLEDevice.h>
#include <sandisolar/arduino_nimble_link.h>

// Replace placeholders with the saved discovery result. No NVS/Wi-Fi required.
const sandisolar::BleTarget target = {
    {0,0,0,0,0,0}, sandisolar::BleAddressType::Public, {"WF00000000",10}};
sandisolar::BleReadSession session(sandisolar::arduino_nimble_link(),
                                 sandisolar::esp32_mbedtls_aes128_cbc());
bool ready=false;
unsigned long nextConnect=0, nextRead=0;
void setup() {
  Serial.begin(115200);
  // If using Wi-Fi, enable modem sleep before initializing BLE.
  if (!NimBLEDevice::init("inverter-reader")) return;
  NimBLEDevice::setMTU(512);
  ready=sandisolar::attach_arduino_nimble()==sandisolar::Error::None;
}
void loop() {
  if (!ready) { delay(1000); return; }
  const auto now=millis();
  session.poll(now);
  sandisolar::BleReadResult result{};
  if (session.take(result)) {
    const auto soc=sandisolar::decode_signal(sandisolar::aohai_modbus_v214_profile(),
                                           sandisolar::SignalId::BatterySoc,result);
    if (soc) Serial.printf("SOC %.0f%%\n",sandisolar::scaled_value(soc.value));
    else Serial.println(sandisolar::error_name(soc.error));
  }
  if (session.state()==sandisolar::BleSessionState::Idle &&
      static_cast<long>(now-nextConnect)>=0) {
    nextConnect=now+60000;
    session.connect(target,now);
  }
  if (session.state()==sandisolar::BleSessionState::Ready &&
      static_cast<long>(now-nextRead)>=0) {
    const auto e=sandisolar::read_signal(session,sandisolar::aohai_modbus_v214_profile(),
                                       sandisolar::SignalId::BatterySoc,now);
    if (e==sandisolar::Error::CorrelationExhausted) session.disconnect(now);
    nextRead=now+10000;
  }
  // Fault means the adapter could not prove a clean close. Do not reset a
  // shared radio here: application-level recovery must consider other clients.
  delay(5);
}
