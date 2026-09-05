#pragma once

#include "haipower/ble_codec.h"

namespace haipower {

enum class BleAddressType : std::uint8_t { Public, Random };
struct BleTarget {
  std::uint8_t address[6];  // display order, not reversed
  BleAddressType address_type;
  BleLoggerIdentity logger;
};
struct BleReadRequest {
  std::uint8_t slave;
  RegisterTable table;
  std::uint16_t start;
  std::uint16_t count;
};
struct BleReadResult {
  Error error;
  BleReadRequest request;
  std::uint32_t epoch;
  std::uint32_t requested_at_ms;
  std::uint32_t received_at_ms;  // callback arrival, NOT inverter sample time
  std::uint16_t registers[kBlePageRegisterCount];  // usable only on success
};

// Technology seam, not a general-purpose BLE API. All methods are bounded,
// non-blocking and called by one owner task. poll consumes at most one event.
// Closed is a lifecycle barrier: no old callback may be relabelled as a new
// epoch. A failed close must report Fault, never optimistic Closed.
class BleLink {
 public:
  virtual ~BleLink() = default;
 protected:
  friend class BleReadSession;
  enum class EventType { Ready, Written, Notification, Closed, Fault };
  struct Event {
    EventType type;
    std::uint32_t epoch;
    std::uint32_t at_ms;
    Error error;
    std::uint16_t mtu;
    std::size_t size;
    std::uint8_t bytes[512];
  };
  virtual Error open(const BleTarget& target, std::uint32_t epoch) = 0;
  virtual Error write(const std::uint8_t* data, std::size_t size) = 0;
  virtual void close() = 0;
  virtual bool poll(Event& event) = 0;
};

enum class BleSessionState { Idle, Connecting, Ready, Reading, Closing, Fault };

// No heap, Wi-Fi, restart, implicit retries or profile selection. The link and
// crypto context must outlive this object. A session exclusively owns its link.
// Call poll frequently from the same task; take() copies an owned result.
// Destruction requires Idle/Fault and no further link use (no async RAII close).
class BleReadSession {
 public:
  BleReadSession(BleLink& link, Aes128Cbc crypto);
  BleReadSession(const BleReadSession&) = delete;
  BleReadSession& operator=(const BleReadSession&) = delete;
  Error connect(const BleTarget& target, std::uint32_t now_ms);
  Error read(BleReadRequest request, std::uint32_t now_ms);
  void disconnect(std::uint32_t now_ms);
  void poll(std::uint32_t now_ms);
  bool take(BleReadResult& result);
  BleSessionState state() const { return state_; }
  Error last_error() const { return last_error_; }
  // Validated but rejected packets are diagnostic only, never cached telemetry.
  Error last_packet_error() const { return last_packet_error_; }

 private:
  void finish(Error error);
  void closing(Error error, std::uint32_t now_ms);
  BleLink& link_;
  Aes128Cbc crypto_;
  BleSessionState state_ = BleSessionState::Idle;
  Error last_error_ = Error::None;
  Error last_packet_error_ = Error::None;
  char logger_[kBleLoggerIdentityMaxSize]{};
  std::size_t logger_size_ = 0;
  std::uint32_t epoch_ = 0;
  std::uint32_t phase_at_ = 0;
  std::uint32_t gap_at_ = 0;
  std::uint16_t correlation_ = 0;  // 255 means reconnect, NEVER reuse in a link
  std::uint16_t mtu_ = 0;
  bool gap_active_ = false;
  bool pending_result_ = false;
  bool written_ = false;
  bool received_ = false;
  BleReadResult result_{};
};

}  // namespace haipower
