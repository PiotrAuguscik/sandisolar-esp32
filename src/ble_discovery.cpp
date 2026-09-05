#include "haipower/ble_discovery.h"

#include <cstring>

namespace haipower {
namespace {
bool valid_candidate(const BleScanCandidate& candidate) {
  bool address_nonzero = false;
  for (auto byte : candidate.address) address_nonzero |= byte != 0;
  return address_nonzero &&
      (candidate.address_type == BleAddressType::Public || candidate.address_type == BleAddressType::Random) &&
      valid_ble_logger_identity({candidate.advertisement.name, candidate.advertisement.name_size});
}

bool parse_ad(const std::uint8_t* p, std::size_t n, BleAdvertisement& out) {
  if (n > 31 || (n && !p)) return false;
  for (std::size_t offset = 0; offset < n;) {
    const auto length = p[offset++];
    if (!length) {
      // Remaining zero padding is allowed, hidden AD structures are not.
      while (offset < n) if (p[offset++]) return false;
      break;
    }
    if (length > n - offset) return false;
    const auto type = p[offset];
    const auto* value = p + offset + 1;
    const std::size_t size = length - 1;
    if (type == 0x09) {
      if (!valid_ble_logger_identity({reinterpret_cast<const char*>(value), size}))
        return false;
      if (out.name_size && (out.name_size != size ||
          std::memcmp(out.name, value, size) != 0)) return false;
      out.name_size = static_cast<std::uint8_t>(size);
      std::memcpy(out.name, value, size);
      out.name[size] = '\0';
    } else if (type == 0x02 || type == 0x03) {
      if (size % 2) return false;
      for (std::size_t i = 0; i < size; i += 2)
        if (value[i] == 0xff && value[i + 1] == 0x00) out.service_00ff = true;
    } else if (type == 0x06 || type == 0x07) {
      if (size % 16) return false;
      // Bluetooth base UUID, little-endian advertising representation.
      const std::uint8_t uuid[] = {0xfb,0x34,0x9b,0x5f,0x80,0,0,0x80,
                                   0,0x10,0,0,0xff,0,0,0};
      for (std::size_t i = 0; i < size; i += 16)
        if (std::memcmp(value + i, uuid, 16) == 0) out.service_00ff = true;
    }
    offset += length;
  }
  return true;
}
}  // namespace

Result<BleAdvertisement> parse_ble_advertisement(
    const std::uint8_t* advertisement, std::size_t advertisement_size,
    const std::uint8_t* scan_response, std::size_t scan_response_size) {
  BleAdvertisement result{};
  if (!parse_ad(advertisement, advertisement_size, result) ||
      !parse_ad(scan_response, scan_response_size, result)) return {Error::BadLength, {}};
  if (!result.name_size) return {Error::NotFound, {}};
  result.wf_name = result.name_size == 10 && result.name[0] == 'W' && result.name[1] == 'F';
  for (std::size_t i = 2; result.wf_name && i < result.name_size; ++i)
    if (result.name[i] < '0' || result.name[i] > '9') result.wf_name = false;
  return {Error::None, result};
}

BleTarget BleScanCandidate::target() const {
  BleTarget result{};
  std::memcpy(result.address, address, sizeof(address));
  result.address_type = address_type;
  result.logger = {advertisement.name, advertisement.name_size};
  return result;
}

void merge_ble_candidate(BleScanResult& result, const BleScanCandidate& candidate) {
  if (result.count > kBleScanCapacity || !valid_candidate(candidate)) {
    result.incomplete = true;
    return;
  }
  for (std::size_t i = 0; i < result.count; ++i) {
    auto& old = result.candidates[i];
    if (!valid_candidate(old)) { result.incomplete = true; return; }
    if (old.address_type != candidate.address_type ||
        std::memcmp(old.address, candidate.address, 6) != 0) continue;
    if (old.advertisement.name_size != candidate.advertisement.name_size ||
        std::memcmp(old.advertisement.name, candidate.advertisement.name,
                    old.advertisement.name_size) != 0) {
      result.incomplete = true;
      return;
    }
    old.rssi = candidate.rssi;
    old.seen_at_ms = candidate.seen_at_ms;
    old.advertisement.service_00ff |= candidate.advertisement.service_00ff;
    return;
  }
  if (result.count == kBleScanCapacity) { result.incomplete = true; return; }
  result.candidates[result.count++] = candidate;
  result.candidates[result.count - 1].advertisement.name[candidate.advertisement.name_size] = '\0';
}

Result<BleScanCandidate> select_ble_candidate(
    const BleScanResult& result, BleLoggerIdentity exact_name) {
  if (exact_name.size && !valid_ble_logger_identity(exact_name))
    return {Error::InvalidArgument, {}};
  if (result.count > kBleScanCapacity) return {Error::InvalidArgument, {}};
  if (result.error != Error::None) return {result.error, {}};
  if (result.incomplete) return {Error::IncompleteScan, {}};
  const BleScanCandidate* match = nullptr;
  for (std::size_t i = 0; i < result.count; ++i) {
    const auto& candidate = result.candidates[i];
    if (!valid_candidate(candidate)) return {Error::InvalidArgument, {}};
    if (exact_name.size && (exact_name.size != candidate.advertisement.name_size ||
        std::memcmp(exact_name.data, candidate.advertisement.name, exact_name.size) != 0)) continue;
    if (match) return {Error::AmbiguousDevice, {}};
    match = &candidate;
  }
  return match ? Result<BleScanCandidate>{Error::None, *match}
               : Result<BleScanCandidate>{Error::NotFound, {}};
}
}  // namespace haipower
