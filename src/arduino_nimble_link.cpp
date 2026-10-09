#include "haipower/arduino_nimble_link.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(SANDISOLAR_USE_NIMBLE)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace haipower {
namespace {
class NimbleLink final : public BleLink {
  enum class Phase { Idle, Active };
  struct Command {
    bool opening;
    std::uint32_t epoch;
    std::uint8_t address[6];
    std::uint8_t address_type;
    std::size_t size;
    std::uint8_t bytes[kBlePageReadFrameSize];
  };
 public:
  Error attach() {
    if (ready_) return Error::None;
    if (attempted_ || !NimBLEDevice::isInitialized()) return Error::RadioConflict;
    wifi_mode_t mode = WIFI_MODE_NULL;
    const auto status = esp_wifi_get_mode(&mode);
    if (status != ESP_OK && status != ESP_ERR_WIFI_NOT_INIT) return Error::RadioConflict;
    if (status == ESP_OK && mode != WIFI_MODE_NULL) {
      wifi_ps_type_t ps = WIFI_PS_NONE;
      if (esp_wifi_get_ps(&ps) != ESP_OK ||
          (ps != WIFI_PS_MIN_MODEM && ps != WIFI_PS_MAX_MODEM)) return Error::RadioConflict;
    }
    attempted_ = true;
    events_ = xQueueCreate(8, sizeof(Event));
    commands_ = xQueueCreate(1, sizeof(Command));
    if (!events_ || !commands_) return Error::TransportFault;
    client_ = NimBLEDevice::createClient();
    if (!client_) return Error::TransportFault;
    client_->setConnectTimeout(10000);
    if (xTaskCreate([](void* p) { static_cast<NimbleLink*>(p)->run(); },
                    "inverter-ble", 6144, this, 1, nullptr) != pdPASS)
      return Error::TransportFault;
    ready_ = true;
    return Error::None;
  }
 protected:
  Error open(const BleTarget& target, std::uint32_t epoch) override {
    if (!ready_) return Error::NotConnected;
    if (NimBLEDevice::getScan()->isScanning()) return Error::Busy;
    Phase expected = Phase::Idle;
    if (!phase_.compare_exchange_strong(expected, Phase::Active)) return Error::Busy;
    // Idle is published only by the worker after disconnect and queue drain.
    xQueueReset(events_);
    overflow_ = false;
    epoch_ = epoch;
    Command c{};
    c.opening = true; c.epoch = epoch;
    std::memcpy(c.address, target.address, sizeof(c.address));
    c.address_type = static_cast<std::uint8_t>(target.address_type);
    if (xQueueSend(commands_, &c, 0) != pdTRUE) {
      phase_ = Phase::Idle;
      return Error::TransportFault;
    }
    return Error::None;
  }
  Error write(const std::uint8_t* data, std::size_t size) override {
    if (!ready_ || phase_ != Phase::Active || closing_) return Error::NotConnected;
    if (!data || !size || size > sizeof(Command::bytes)) return Error::InvalidArgument;
    Command c{};
    c.epoch = epoch_; c.size = size;
    std::memcpy(c.bytes, data, size);
    return xQueueSend(commands_, &c, 0) == pdTRUE ? Error::None : Error::Busy;
  }
  void close() override { if (ready_ && phase_ == Phase::Active) closing_ = true; }
  bool poll(Event& event) override {
    if (!ready_) return false;
    if (overflow_.exchange(false)) {
      closing_ = true;
      event = {}; event.type = EventType::Fault; event.epoch = epoch_;
      event.at_ms = millis(); event.error = Error::TransportFault;
      return true;
    }
    return xQueueReceive(events_, &event, 0) == pdTRUE;
  }
 private:
  void emit(EventType type, std::uint32_t epoch, Error error = Error::None) {
    Event e{}; e.type = type; e.epoch = epoch; e.error = error;
    e.at_ms = millis(); e.mtu = client_->getMTU();
    if (xQueueSend(events_, &e, 0) != pdTRUE) overflow_ = true;
  }
  void stop(std::uint32_t epoch) {
    accepting_ = false;
    if (client_->isConnected()) client_->disconnect();
    const auto start = millis();
    while (client_->isConnected() && std::uint32_t(millis()-start) < 3000) delay(10);
    if (client_->isConnected()) {
      // No optimistic Closed, no new connection epoch on a live old client.
      emit(EventType::Fault, epoch, Error::TransportFault);
      return;
    }
    characteristic_ = nullptr;
    xQueueReset(commands_);
    closing_ = false;
    phase_ = Phase::Idle;
    emit(EventType::Closed, epoch, close_error_);
    close_error_ = Error::None;
  }
  void connect(const Command& c) {
    char address[18];
    std::snprintf(address, sizeof(address), "%02x:%02x:%02x:%02x:%02x:%02x",
                  c.address[0],c.address[1],c.address[2],c.address[3],c.address[4],c.address[5]);
    if (!client_->connect(NimBLEAddress(std::string(address), c.address_type), true, false, true)) {
      close_error_ = Error::TransportFailure; closing_ = true; return;
    }
    auto* service = client_->getService(NimBLEUUID(std::uint16_t(0x00ff)));
    characteristic_ = service ? service->getCharacteristic(NimBLEUUID(std::uint16_t(0xff01))) : nullptr;
    if (!characteristic_ || !characteristic_->canNotify() || !characteristic_->canWrite()) {
      close_error_ = Error::TransportFailure; closing_ = true; return;
    }
    const auto epoch = c.epoch;  // immutable callback identity, never current epoch
    accepting_ = true;
    if (!characteristic_->subscribe(true,
        [this, epoch](NimBLERemoteCharacteristic*, std::uint8_t* data, std::size_t size, bool) {
          if (!accepting_ || epoch != epoch_.load()) return;
          Event e{}; e.type = EventType::Notification; e.epoch = epoch; e.at_ms = millis();
          if (size > sizeof(e.bytes)) { overflow_ = true; return; }
          e.size = size; std::memcpy(e.bytes, data, size);
          if (xQueueSend(events_, &e, 0) != pdTRUE) overflow_ = true;
        }, true)) {
      close_error_ = Error::TransportFailure; closing_ = true; return;
    }
    if (!closing_) emit(EventType::Ready, epoch);
  }
  void run() {
    // Only this worker touches GATT pointers or performs blocking stack calls.
    for (;;) {
      if (closing_) { stop(epoch_); delay(10); continue; }
      Command c{};
      if (xQueueReceive(commands_, &c, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (c.opening) connect(c);
        else if (!closing_ && c.epoch == epoch_ && client_->isConnected() && characteristic_) {
          const bool ok = characteristic_->writeValue(c.bytes, c.size, true);
          emit(EventType::Written, c.epoch,
               ok ? Error::None : Error::TransportFailure);
          if (!ok) { close_error_ = Error::TransportFailure; closing_ = true; }
        }
      } else if (phase_ == Phase::Active && accepting_ && !client_->isConnected()) {
        close_error_ = Error::NotConnected;
        stop(epoch_);
      }
    }
  }
  bool attempted_ = false, ready_ = false;
  std::atomic<Phase> phase_{Phase::Idle};
  std::atomic<bool> accepting_{false}, closing_{false}, overflow_{false};
  std::atomic<std::uint32_t> epoch_{0};
  NimBLEClient* client_ = nullptr;
  NimBLERemoteCharacteristic* characteristic_ = nullptr;
  Error close_error_ = Error::None;  // worker-owned; published only after close barrier
  QueueHandle_t events_ = nullptr, commands_ = nullptr;
};
NimbleLink& link() { static NimbleLink value; return value; }
}  // namespace
Error attach_arduino_nimble() { return link().attach(); }
BleLink& arduino_nimble_link() { return link(); }
}  // namespace haipower
#endif
