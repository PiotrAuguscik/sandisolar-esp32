#pragma once

#include "haipower/ble_session.h"

namespace haipower {

// Discovery hints, NOT authenticated identity or model qualification.
struct BleAdvertisement {
  char name[kBleLoggerIdentityMaxSize + 1];
  std::uint8_t name_size;
  bool service_00ff;
  bool wf_name;  // current logger naming convention: WF followed by eight digits
};

// Parse complete local name (AD 09) and 16-bit/128-bit service UUID lists.
// Shortened names are not usable as the encrypted protocol identity.
Result<BleAdvertisement> parse_ble_advertisement(
    const std::uint8_t* advertisement, std::size_t advertisement_size,
    const std::uint8_t* scan_response, std::size_t scan_response_size);

constexpr std::size_t kBleScanCapacity = 16;
struct BleScanCandidate {
  std::uint8_t address[6];
  BleAddressType address_type;
  BleAdvertisement advertisement;
  std::int16_t rssi;
  std::uint32_t seen_at_ms;

  // Borrowed name points into THIS candidate; connect() immediately copies it.
  // Keep the candidate alive until connect returns. Copies own their own name.
  BleTarget target() const;
};

struct BleScanResult {
  Error error;
  std::uint32_t started_at_ms;
  std::uint32_t completed_at_ms;
  std::size_t count;
  // Lost candidate reports, conflicting identities, or full candidate storage.
  // Selection rejects incomplete results, even with an exact name supplied.
  bool incomplete;
  BleScanCandidate candidates[kBleScanCapacity];
};

// Merge by address AND address type; changed complete names mark uncertainty.
void merge_ble_candidate(BleScanResult& result, const BleScanCandidate& candidate);

// No strongest-RSSI/first-device fallback. Empty name requires exactly one
// candidate. An exact logger name can disambiguate several different loggers.
Result<BleScanCandidate> select_ble_candidate(
    const BleScanResult& result, BleLoggerIdentity exact_name = {nullptr, 0});

enum class BleScanState { Idle, Configuring, Starting, Scanning, Complete, Fault };

// One owner task; finite active scan, no connections or configuration writes.
// A completed result is owned and must be taken before another scan/connect.
// No early-cancel API: wait for the finite scan. Missing completion is terminal.
class BleScanner {
 public:
  virtual ~BleScanner() = default;
  // 1..60 seconds. Optional complete name is copied for this scan, and allows
  // explicit discovery of a known name outside the default WF/00ff hints.
  virtual Error start(std::uint32_t duration_seconds, std::uint32_t now_ms,
                      BleLoggerIdentity exact_name = {nullptr, 0}) = 0;
  virtual void poll(std::uint32_t now_ms) = 0;
  virtual bool take(BleScanResult& result) = 0;
  virtual BleScanState state() const = 0;
};

}  // namespace haipower
