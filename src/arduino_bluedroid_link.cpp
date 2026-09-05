#include "haipower/arduino_bluedroid_link.h"

#if defined(ARDUINO_ARCH_ESP32) && ESP_IDF_VERSION_MAJOR == 4
#include <Arduino.h>
#include <esp32-hal-bt.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstring>

namespace haipower {
namespace {
constexpr std::uint16_t kApplication = 0x4841;
constexpr std::uint16_t kNoConnection = 0xffff;
enum class Phase { Idle, Registering, Opening, Mtu, Search, Notify, Cccd,
                   Ready, Writing, Closing, Unregistering, Fault };

// Own callback bytes: ESP notification pointers expire on callback return.
struct RawEvent {
  esp_gattc_cb_event_t type;
  esp_gatt_if_t interface;
  std::uint16_t conn;
  std::uint16_t status;
  std::uint16_t handle;
  std::uint16_t end;
  std::uint16_t mtu;
  std::uint16_t size;
  std::uint32_t at_ms;
  std::uint8_t address[6];
  std::uint8_t bytes[512];
};

enum class ScanControlType { Configured, Started, Finished };
struct ScanControl {
  ScanControlType type;
  std::uint16_t status;
  std::uint32_t at_ms;
};

esp_bt_uuid_t uuid16(std::uint16_t value) {
  esp_bt_uuid_t uuid{};
  uuid.len = ESP_UUID_LEN_16;
  uuid.uuid.uuid16 = value;
  return uuid;
}

class BluedroidLink final : public BleLink, public BleScanner {
 public:
  Error initialize();
  static void callback(esp_gattc_cb_event_t type, esp_gatt_if_t interface,
                       esp_ble_gattc_cb_param_t* param);
  static void gap_callback(esp_gap_ble_cb_event_t type, esp_ble_gap_cb_param_t* param);
 private:
  Error start(std::uint32_t seconds, std::uint32_t now,
              BleLoggerIdentity exact_name) override;
  void poll(std::uint32_t now) override;
  bool take(BleScanResult& result) override;
  BleScanState state() const override { return scan_state_; }
  void scan_fault(Error error, std::uint32_t now);
  Error open(const BleTarget& target, std::uint32_t epoch) override;
  Error write(const std::uint8_t* data, std::size_t size) override;
  void close() override;
  bool poll(Event& event) override;
  void unregister_app();
  void fail(Error error);
  bool emit(Event& event, EventType type, Error error = Error::None);
  QueueHandle_t queue_ = nullptr;  // retained for process lifetime
  std::atomic<bool> overflow_{false};
  bool initialized_ = false;
  bool attempted_ = false;
  bool close_requested_ = false;
  bool fault_reported_ = false;
  Phase phase_ = Phase::Idle;
  Error close_error_ = Error::None;
  esp_gatt_if_t interface_ = ESP_GATT_IF_NONE;
  std::uint16_t conn_ = kNoConnection;
  std::uint16_t service_start_ = 0;
  std::uint16_t service_end_ = 0;
  std::uint16_t characteristic_ = 0;
  std::uint16_t cccd_ = 0;
  std::uint16_t mtu_ = 23;
  std::uint32_t epoch_ = 0;
  std::uint8_t address_[6]{};
  esp_ble_addr_type_t address_type_ = BLE_ADDR_TYPE_PUBLIC;
  // Separate bounded queues: nearby advertisements cannot evict GAP lifecycle
  // evidence or GATT notifications. The same owner task drives both APIs.
  QueueHandle_t scan_controls_ = nullptr;
  QueueHandle_t scan_advertisements_ = nullptr;
  std::atomic<bool> scan_accepting_{false};
  std::atomic<bool> scan_control_lost_{false};
  std::atomic<bool> scan_ad_lost_{false};
  BleScanState scan_state_ = BleScanState::Idle;
  BleScanResult scan_result_{};
  esp_ble_scan_params_t scan_params_{};  // alive through parameter-set callback
  char scan_name_[kBleLoggerIdentityMaxSize]{};
  std::size_t scan_name_size_ = 0;
  std::uint32_t scan_seconds_ = 0;
  std::uint32_t scan_phase_at_ = 0;
  bool scan_done_ = false;
  bool scan_pending_ = false;
};

BluedroidLink& singleton() {
  static BluedroidLink instance;
  return instance;
}

Error BluedroidLink::initialize() {
  if (initialized_) return Error::None;
  if (attempted_) return Error::TransportFault;
  // Never replace another library's callback or deinitialize someone else's BT.
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED)
    return Error::Busy;
  attempted_ = true;
  queue_ = xQueueCreate(8, sizeof(RawEvent));
  scan_controls_ = xQueueCreate(4, sizeof(ScanControl));
  scan_advertisements_ = xQueueCreate(16, sizeof(BleScanCandidate));
  if (!queue_ || !scan_controls_ || !scan_advertisements_ ||
      !btStart() || esp_bluedroid_init() != ESP_OK ||
      esp_bluedroid_enable() != ESP_OK ||
      esp_ble_gattc_register_callback(callback) != ESP_OK ||
      esp_ble_gap_register_callback(gap_callback) != ESP_OK ||
      esp_ble_gatt_set_local_mtu(512) != ESP_OK) {
    phase_ = Phase::Fault;
    return Error::TransportFault;
  }
  initialized_ = true;
  return Error::None;
}

void BluedroidLink::callback(esp_gattc_cb_event_t type, esp_gatt_if_t interface,
                              esp_ble_gattc_cb_param_t* p) {
  // IDF 4.4 btc_gattc_cb_handler supplies NULL for ESP_GATTC_UNREG_EVT.
  // Dropping it would destroy the only confirmed re-connect barrier.
  if (!p && type != ESP_GATTC_UNREG_EVT) return;
  RawEvent e{};
  e.type = type;
  e.interface = interface;
  e.conn = kNoConnection;
  e.at_ms = millis();
  switch (type) {
    case ESP_GATTC_REG_EVT:
      if (p->reg.app_id != kApplication) return;
      e.status = p->reg.status;
      break;
    case ESP_GATTC_UNREG_EVT: break;
    case ESP_GATTC_OPEN_EVT:
      e.status = p->open.status;
      e.conn = p->open.conn_id;
      std::memcpy(e.address, p->open.remote_bda, 6);
      break;
    case ESP_GATTC_CLOSE_EVT:
      e.status = p->close.status;
      e.conn = p->close.conn_id;
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      e.conn = p->disconnect.conn_id;
      std::memcpy(e.address, p->disconnect.remote_bda, 6);
      break;
    case ESP_GATTC_CFG_MTU_EVT:
      e.status = p->cfg_mtu.status;
      e.conn = p->cfg_mtu.conn_id;
      e.mtu = p->cfg_mtu.mtu;
      break;
    case ESP_GATTC_SEARCH_RES_EVT:
      if (p->search_res.srvc_id.uuid.len != ESP_UUID_LEN_16 ||
          p->search_res.srvc_id.uuid.uuid.uuid16 != 0x00ff) return;
      e.conn = p->search_res.conn_id;
      e.handle = p->search_res.start_handle;
      e.end = p->search_res.end_handle;
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
      e.conn = p->search_cmpl.conn_id;
      e.status = p->search_cmpl.status;
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      e.status = p->reg_for_notify.status;
      e.handle = p->reg_for_notify.handle;
      break;
    case ESP_GATTC_WRITE_DESCR_EVT:
    case ESP_GATTC_WRITE_CHAR_EVT:
      e.status = p->write.status;
      e.conn = p->write.conn_id;
      e.handle = p->write.handle;
      break;
    case ESP_GATTC_NOTIFY_EVT:
      e.conn = p->notify.conn_id;
      e.handle = p->notify.handle;
      e.size = p->notify.value_len;
      std::memcpy(e.address, p->notify.remote_bda, 6);
      if (e.size > sizeof(e.bytes) || (e.size && !p->notify.value)) {
        singleton().overflow_.store(true);
        return;
      }
      if (e.size) std::memcpy(e.bytes, p->notify.value, e.size);
      break;
    case ESP_GATTC_SRVC_CHG_EVT:
      std::memcpy(e.address, p->srvc_chg.remote_bda, 6);
      break;
    default: return;
  }
  auto& self = singleton();
  if (!self.queue_ || xQueueSend(self.queue_, &e, 0) != pdTRUE)
    self.overflow_.store(true);
}

Error BluedroidLink::open(const BleTarget& target, std::uint32_t epoch) {
  if (!initialized_ || phase_ == Phase::Fault) return Error::TransportFault;
  if (phase_ != Phase::Idle || scan_state_ != BleScanState::Idle) return Error::Busy;
  std::memcpy(address_, target.address, 6);
  address_type_ = target.address_type == BleAddressType::Public ?
      BLE_ADDR_TYPE_PUBLIC : BLE_ADDR_TYPE_RANDOM;
  epoch_ = epoch;
  interface_ = ESP_GATT_IF_NONE;
  conn_ = kNoConnection;
  service_start_ = service_end_ = characteristic_ = cccd_ = 0;
  close_requested_ = false;
  close_error_ = Error::None;
  phase_ = Phase::Registering;
  if (esp_ble_gattc_app_register(kApplication) != ESP_OK) {
    phase_ = Phase::Idle;
    return Error::TransportFailure;
  }
  return Error::None;
}

Error BluedroidLink::start(std::uint32_t seconds, std::uint32_t now,
                           BleLoggerIdentity name) {
  if (!seconds || seconds > 60 || (name.size && !valid_ble_logger_identity(name)))
    return Error::InvalidArgument;
  if (!initialized_ || phase_ == Phase::Fault || scan_state_ == BleScanState::Fault)
    return Error::TransportFault;
  if (phase_ != Phase::Idle || scan_state_ != BleScanState::Idle) return Error::Busy;
  scan_result_ = {};
  scan_result_.started_at_ms = now;
  scan_seconds_ = seconds;
  scan_phase_at_ = now;
  scan_done_ = scan_pending_ = false;
  scan_ad_lost_.store(false);
  scan_name_size_ = name.size;
  if (name.size) std::memcpy(scan_name_, name.data, name.size);
  scan_params_ = {};
  scan_params_.scan_type = BLE_SCAN_TYPE_ACTIVE;
  scan_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  scan_params_.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scan_params_.scan_interval = 160;  // 100 ms; coexistence remains unqualified
  scan_params_.scan_window = 80;     // 50 ms
  scan_params_.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  scan_state_ = BleScanState::Configuring;
  scan_accepting_.store(true);  // publish immutable filter before SDK callbacks
  if (esp_ble_gap_set_scan_params(&scan_params_) != ESP_OK) {
    scan_accepting_.store(false);
    scan_state_ = BleScanState::Idle;
    return Error::TransportFailure;
  }
  return Error::None;
}

void BluedroidLink::gap_callback(esp_gap_ble_cb_event_t type,
                                esp_ble_gap_cb_param_t* p) {
  if (!p) return;
  auto& self = singleton();
  if (!self.scan_accepting_.load()) return;
  ScanControl control{};
  control.at_ms = millis();
  if (type == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    control.type = ScanControlType::Configured;
    control.status = p->scan_param_cmpl.status;
  } else if (type == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
    control.type = ScanControlType::Started;
    control.status = p->scan_start_cmpl.status;
  } else if (type == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    const auto& report = p->scan_rst;
    if (report.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
      control.type = ScanControlType::Finished;
      self.scan_accepting_.store(false);
    } else if (report.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
      if (report.adv_data_len > 31 || report.scan_rsp_len > 31 ||
          (report.ble_addr_type != BLE_ADDR_TYPE_PUBLIC &&
           report.ble_addr_type != BLE_ADDR_TYPE_RANDOM)) return;
      auto parsed = parse_ble_advertisement(report.ble_adv, report.adv_data_len,
          report.ble_adv + report.adv_data_len, report.scan_rsp_len);
      if (!parsed) return;
      const auto& ad = parsed.value;
      if (self.scan_name_size_) {
        if (ad.name_size != self.scan_name_size_ ||
            std::memcmp(ad.name, self.scan_name_, ad.name_size) != 0) return;
      } else if (!ad.wf_name && !ad.service_00ff) return;
      BleScanCandidate candidate{};
      std::memcpy(candidate.address, report.bda, 6);
      candidate.address_type = report.ble_addr_type == BLE_ADDR_TYPE_PUBLIC ?
          BleAddressType::Public : BleAddressType::Random;
      candidate.advertisement = ad;
      candidate.rssi = static_cast<std::int16_t>(report.rssi);
      candidate.seen_at_ms = control.at_ms;
      if (xQueueSend(self.scan_advertisements_, &candidate, 0) != pdTRUE)
        self.scan_ad_lost_.store(true);
      return;
    } else return;
  } else return;
  if (xQueueSend(self.scan_controls_, &control, 0) != pdTRUE)
    self.scan_control_lost_.store(true);
}

void BluedroidLink::scan_fault(Error error, std::uint32_t now) {
  scan_accepting_.store(false);
  esp_ble_gap_stop_scanning();  // best effort only; NEVER licenses reuse
  scan_state_ = BleScanState::Fault;
  phase_ = Phase::Fault;       // lost radio lifecycle also fences GATT opens
  scan_result_.error = error;
  scan_result_.incomplete = true;
  scan_result_.completed_at_ms = now;
  scan_pending_ = true;
}

void BluedroidLink::poll(std::uint32_t now) {
  if (scan_state_ == BleScanState::Idle || scan_state_ == BleScanState::Complete ||
      scan_state_ == BleScanState::Fault) return;
  const auto limit = scan_state_ == BleScanState::Scanning ?
      scan_seconds_ * 1000U + 5000U : 10000U;
  if (scan_control_lost_.load()) { scan_fault(Error::TransportFault, now); return; }
  if (now - scan_phase_at_ >= limit) { scan_fault(Error::Timeout, now); return; }
  ScanControl control{};
  // Process all bounded controls, then drain advertisements before publishing
  // completion. Queue separation cannot turn a partial list into a unique peer.
  for (unsigned i = 0; i < 4 && xQueueReceive(scan_controls_, &control, 0) == pdTRUE; ++i) {
    if (control.type == ScanControlType::Configured && scan_state_ == BleScanState::Configuring) {
      if (control.status != ESP_BT_STATUS_SUCCESS) {
        scan_result_.error = Error::TransportFailure;
        scan_done_ = true;
        scan_accepting_.store(false);
      } else {
        scan_state_ = BleScanState::Starting;
        scan_phase_at_ = now;
        if (esp_ble_gap_start_scanning(scan_seconds_) != ESP_OK) {
          scan_result_.error = Error::TransportFailure;
          scan_done_ = true;
          scan_accepting_.store(false);
        }
      }
    } else if (control.type == ScanControlType::Started && scan_state_ == BleScanState::Starting) {
      if (control.status != ESP_BT_STATUS_SUCCESS) {
        scan_result_.error = Error::TransportFailure;
        scan_done_ = true;
        scan_accepting_.store(false);
      } else {
        scan_state_ = BleScanState::Scanning;
        scan_phase_at_ = control.at_ms;
      }
    } else if (control.type == ScanControlType::Finished && scan_state_ == BleScanState::Scanning) {
      scan_result_.completed_at_ms = control.at_ms;
      scan_done_ = true;
    } else {
      scan_fault(Error::TransportFault, now);  // unexpected/duplicate lifecycle
      return;
    }
  }
  BleScanCandidate candidate{};
  for (unsigned i = 0; i < 16 && xQueueReceive(scan_advertisements_, &candidate, 0) == pdTRUE; ++i)
    merge_ble_candidate(scan_result_, candidate);
  if (scan_done_) {
    scan_result_.incomplete |= scan_ad_lost_.load();
    if (scan_result_.error != Error::None) scan_result_.completed_at_ms = now;
    scan_state_ = BleScanState::Complete;
    scan_pending_ = true;
  }
}

bool BluedroidLink::take(BleScanResult& result) {
  if (!scan_pending_) return false;
  result = scan_result_;
  scan_pending_ = false;
  if (scan_state_ == BleScanState::Complete) scan_state_ = BleScanState::Idle;
  return true;
}

Error BluedroidLink::write(const std::uint8_t* data, std::size_t size) {
  if (phase_ != Phase::Ready) return Error::NotConnected;
  if (!data || size > mtu_ - 3U) return Error::MtuTooSmall;
  phase_ = Phase::Writing;
  // IDF copies the supplied bytes into its BTC message before returning.
  if (esp_ble_gattc_write_char(interface_, conn_, characteristic_, size,
      const_cast<std::uint8_t*>(data), ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE) != ESP_OK) return Error::TransportFailure;
  return Error::None;
}

void BluedroidLink::unregister_app() {
  if (phase_ == Phase::Unregistering) return;
  phase_ = Phase::Unregistering;
  if (esp_ble_gattc_app_unregister(interface_) != ESP_OK) phase_ = Phase::Fault;
}

void BluedroidLink::close() {
  close_requested_ = true;
  if (phase_ == Phase::Idle || phase_ == Phase::Closing ||
      phase_ == Phase::Unregistering || phase_ == Phase::Fault) return;
  // Do not race an outstanding registration/open. Their completion either
  // permits a real close or the session reaches its terminal close timeout.
  if (phase_ == Phase::Registering || phase_ == Phase::Opening) return;
  phase_ = Phase::Closing;
  if (esp_ble_gattc_close(interface_, conn_) != ESP_OK) phase_ = Phase::Fault;
}

void BluedroidLink::fail(Error error) {
  close_error_ = error;
  close();
}

bool BluedroidLink::emit(Event& event, EventType type, Error error) {
  event = {};
  event.type = type;
  event.epoch = epoch_;
  event.at_ms = millis();
  event.error = error;
  event.mtu = mtu_;
  return true;
}

bool BluedroidLink::poll(Event& event) {
  if (overflow_.load()) {
    // Lost lifecycle evidence is terminal. Best-effort physical disconnect,
    // never a new connection or optimistic recovery after a queue overflow.
    if (phase_ != Phase::Fault) {
      if (conn_ != kNoConnection) esp_ble_gap_disconnect(address_);
      phase_ = Phase::Fault;
    }
  }
  if (phase_ == Phase::Fault) {
    if (fault_reported_) return false;
    fault_reported_ = true;
    return emit(event, EventType::Fault, Error::TransportFault);
  }
  RawEvent e{};
  if (!queue_ || xQueueReceive(queue_, &e, 0) != pdTRUE) return false;
  if (e.type == ESP_GATTC_REG_EVT && phase_ == Phase::Registering) {
    if (e.status != ESP_GATT_OK) {
      phase_ = Phase::Idle;
      return emit(event, EventType::Closed, Error::TransportFailure);
    }
    interface_ = e.interface;
    if (close_requested_) unregister_app();
    else {
      phase_ = Phase::Opening;
      if (esp_ble_gattc_open(interface_, address_, address_type_, true) != ESP_OK) {
        close_error_ = Error::TransportFailure;
        unregister_app();
      }
    }
    return false;
  }
  if (e.interface != interface_ || interface_ == ESP_GATT_IF_NONE) return false;
  if (e.type == ESP_GATTC_UNREG_EVT && phase_ == Phase::Unregistering) {
    // The serialized IDF callback stream has now retired this GATT application.
    // Only this barrier admits a new epoch; do not reset/drain queues early.
    interface_ = ESP_GATT_IF_NONE;
    conn_ = kNoConnection;
    phase_ = Phase::Idle;
    return emit(event, EventType::Closed, close_error_);
  }
  if (e.type == ESP_GATTC_OPEN_EVT && phase_ == Phase::Opening) {
    if (e.status != ESP_GATT_OK) {
      close_error_ = Error::TransportFailure;
      unregister_app();
    } else if (std::memcmp(e.address, address_, 6) != 0) {
      phase_ = Phase::Fault;
    } else {
      conn_ = e.conn;
      phase_ = Phase::Mtu;
      if (close_requested_) close();
      else if (esp_ble_gattc_send_mtu_req(interface_, conn_) != ESP_OK)
        fail(Error::TransportFailure);
    }
    return false;
  }
  if (e.conn != kNoConnection && e.conn != conn_) return false;
  if ((e.type == ESP_GATTC_CLOSE_EVT || e.type == ESP_GATTC_DISCONNECT_EVT) &&
      phase_ != Phase::Idle && phase_ != Phase::Unregistering) {
    if (e.type == ESP_GATTC_CLOSE_EVT && e.status != ESP_GATT_OK) phase_ = Phase::Fault;
    else {
      if (!close_requested_) close_error_ = Error::NotConnected;
      unregister_app();
    }
    return false;
  }
  if (phase_ == Phase::Closing || phase_ == Phase::Unregistering) return false;
  if (e.type == ESP_GATTC_SRVC_CHG_EVT && std::memcmp(e.address, address_, 6) == 0) {
    fail(Error::TransportFailure);
    return false;
  }
  switch (e.type) {
    case ESP_GATTC_CFG_MTU_EVT:
      if (phase_ != Phase::Mtu) break;
      if (e.status != ESP_GATT_OK || e.mtu < 23) { fail(Error::TransportFailure); break; }
      mtu_ = e.mtu;
      phase_ = Phase::Search;
      {
        auto service = uuid16(0x00ff);
        if (esp_ble_gattc_search_service(interface_, conn_, &service) != ESP_OK)
          fail(Error::TransportFailure);
      }
      break;
    case ESP_GATTC_SEARCH_RES_EVT:
      if (phase_ == Phase::Search) {
        if (service_start_) { fail(Error::TransportFailure); break; }
        service_start_ = e.handle;
        service_end_ = e.end;
      }
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (phase_ == Phase::Search) {
        esp_gattc_char_elem_t chars[2]{};
        std::uint16_t count = 2;
        if (e.status != ESP_GATT_OK || !service_start_ ||
            esp_ble_gattc_get_char_by_uuid(interface_, conn_, service_start_,
              service_end_, uuid16(0xff01), chars, &count) != ESP_GATT_OK ||
            count != 1 || !(chars[0].properties & ESP_GATT_CHAR_PROP_BIT_WRITE) ||
            !(chars[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
          fail(Error::TransportFailure); break;
        }
        characteristic_ = chars[0].char_handle;
        phase_ = Phase::Notify;
        if (esp_ble_gattc_register_for_notify(interface_, address_, characteristic_) != ESP_OK)
          fail(Error::TransportFailure);
      }
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (phase_ == Phase::Notify && e.handle == characteristic_) {
        esp_gattc_descr_elem_t desc[2]{};
        std::uint16_t count = 2;
        if (e.status != ESP_GATT_OK ||
            esp_ble_gattc_get_descr_by_char_handle(interface_, conn_, characteristic_,
              uuid16(0x2902), desc, &count) != ESP_GATT_OK || count != 1) {
          fail(Error::TransportFailure); break;
        }
        cccd_ = desc[0].handle;
        phase_ = Phase::Cccd;
        std::uint8_t enable_notify[] = {1, 0};
        if (esp_ble_gattc_write_char_descr(interface_, conn_, cccd_, 2, enable_notify,
              ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE) != ESP_OK)
          fail(Error::TransportFailure);
      }
      break;
    case ESP_GATTC_WRITE_DESCR_EVT:
      if (phase_ == Phase::Cccd && e.handle == cccd_) {
        if (e.status != ESP_GATT_OK) { fail(Error::TransportFailure); break; }
        phase_ = Phase::Ready;
        return emit(event, EventType::Ready);
      }
      break;
    case ESP_GATTC_WRITE_CHAR_EVT:
      if (phase_ == Phase::Writing && e.handle == characteristic_) {
        phase_ = Phase::Ready;
        return emit(event, EventType::Written,
            e.status == ESP_GATT_OK ? Error::None : Error::TransportFailure);
      }
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if ((phase_ == Phase::Ready || phase_ == Phase::Writing) &&
          e.handle == characteristic_ && std::memcmp(e.address, address_, 6) == 0) {
        emit(event, EventType::Notification);
        event.at_ms = e.at_ms;
        event.size = e.size;
        std::memcpy(event.bytes, e.bytes, e.size);
        return true;
      }
      break;
    default: break;
  }
  return false;
}
}  // namespace

Error initialize_arduino_bluedroid() { return singleton().initialize(); }
BleLink& arduino_bluedroid_link() { return singleton(); }
BleScanner& arduino_bluedroid_scanner() { return singleton(); }
}  // namespace haipower
#endif
