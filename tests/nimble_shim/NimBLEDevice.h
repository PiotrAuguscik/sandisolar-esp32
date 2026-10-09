#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <cstdint>
class NimBLEUUID { public: explicit NimBLEUUID(std::uint16_t) {} };
class NimBLEAddress { public: NimBLEAddress(std::string,std::uint8_t) {} };
class NimBLERemoteCharacteristic {
 public:
  using Notify=std::function<void(NimBLERemoteCharacteristic*,std::uint8_t*,std::size_t,bool)>;
  Notify callback;
  bool canNotify() const { return true; }
  bool canWrite() const { return true; }
  bool subscribe(bool,Notify cb,bool) { callback=cb; return true; }
  bool writeValue(const std::uint8_t*,std::size_t,bool) { return true; }
};
class NimBLERemoteService {
 public:
  NimBLERemoteCharacteristic characteristic;
  NimBLERemoteCharacteristic* getCharacteristic(NimBLEUUID) { return &characteristic; }
};
class NimBLEClient {
 public:
  std::atomic<bool> connected{false};
  bool connectSucceeds=true;
  NimBLERemoteService service;
  void setConnectTimeout(unsigned) {}
  bool connect(NimBLEAddress,bool,bool,bool) { connected=connectSucceeds; return connectSucceeds; }
  void disconnect() { connected=false; }
  bool isConnected() const { return connected; }
  unsigned getMTU() const { return 512; }
  NimBLERemoteService* getService(NimBLEUUID) { return &service; }
};
class NimBLEScan { public: bool isScanning() const { return false; } };
extern NimBLEClient test_client;
extern NimBLEScan test_scan;
extern bool test_initialized;
class NimBLEDevice {
 public:
  static bool isInitialized() { return test_initialized; }
  static NimBLEClient* createClient() { return &test_client; }
  static NimBLEScan* getScan() { return &test_scan; }
  // Deliberately no init/deinit/setMTU: adapter must not own shared runtime.
};
