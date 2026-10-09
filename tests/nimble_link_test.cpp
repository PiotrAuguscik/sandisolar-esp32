#include <cassert>
#include <cstdlib>
#include <NimBLEDevice.h>
#include <Arduino.h>
// Exercise the concrete transport contract directly, without crypto fixtures.
#define protected public
#include "haipower/arduino_nimble_link.h"
#undef protected
NimBLEClient test_client;
NimBLEScan test_scan;
bool test_initialized=false;
int test_ps=0;
using namespace haipower;
BleLink::Event next(BleLink::EventType type) {
  const auto start=millis(); BleLink::Event event{};
  while (millis()-start<2000) {
    if (arduino_nimble_link().poll(event)) { assert(event.type==type); return event; }
    delay(1);
  }
  assert(false && "missing event"); return event;
}
int main() {
  assert(attach_arduino_nimble()==Error::RadioConflict);
  test_initialized=true;
  assert(attach_arduino_nimble()==Error::RadioConflict);
  test_ps=1;
  assert(attach_arduino_nimble()==Error::None);
  auto& link=arduino_nimble_link();
  BleTarget target{{1,2,3,4,5,6},BleAddressType::Public,{"WF00000000",10}};
  assert(link.open(target,1)==Error::None);
  assert(next(BleLink::EventType::Ready).epoch==1);
  auto oldCallback=test_client.service.characteristic.callback;
  const std::uint8_t request[]={1,2};
  assert(link.write(request,sizeof(request))==Error::None);
  assert(next(BleLink::EventType::Written).epoch==1);
  std::uint8_t payload[]={7,8,9};
  oldCallback(nullptr,payload,sizeof(payload),true);
  payload[0]=0;
  auto received=next(BleLink::EventType::Notification);
  assert(received.bytes[0]==7 && received.size==3);
  link.close(); assert(next(BleLink::EventType::Closed).epoch==1);
  assert(!test_client.isConnected());
  test_client.connectSucceeds=false;
  assert(link.open(target,2)==Error::None);
  auto unavailable=next(BleLink::EventType::Closed);
  assert(unavailable.epoch==2 && unavailable.error==Error::TransportFailure);
  test_client.connectSucceeds=true;
  assert(link.open(target,3)==Error::None);
  assert(next(BleLink::EventType::Ready).epoch==3);
  oldCallback(nullptr,payload,sizeof(payload),true);
  BleLink::Event event{}; assert(!link.poll(event));
  test_client.connected=false;
  auto disconnected=next(BleLink::EventType::Closed);
  assert(disconnected.epoch==3 && disconnected.error==Error::NotConnected);
  assert(link.open(target,4)==Error::None);
  assert(next(BleLink::EventType::Ready).epoch==4);
  // Bounded queue saturation is a transport fault, not silently lost telemetry.
  for (int i=0;i<9;++i) test_client.service.characteristic.callback(nullptr,payload,3,true);
  assert(next(BleLink::EventType::Fault).error==Error::TransportFault);
  // Worker lifetime intentionally equals process lifetime, just as on ESP32.
  std::_Exit(0);
}
