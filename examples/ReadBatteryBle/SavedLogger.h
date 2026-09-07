#pragma once
#include <sandisolar.h>
#include <cstring>

// Example-local persistence format, not a dump of a C++ struct or BLE handles.
// One versioned blob keeps name, address and address type together in NVS.
namespace saved_logger {
constexpr std::size_t size = 44;
inline bool valid(const sandisolar::BleScanCandidate& candidate) {
  if (candidate.address_type != sandisolar::BleAddressType::Public &&
      candidate.address_type != sandisolar::BleAddressType::Random) return false;
  unsigned any = 0, all = 255;
  for (auto byte : candidate.address) { any |= byte; all &= byte; }
  return any && all != 255 && sandisolar::valid_ble_logger_identity(candidate.target().logger);
}
inline bool encode(const sandisolar::BleScanCandidate& candidate, std::uint8_t (&bytes)[size]) {
  if (!valid(candidate)) return false;
  std::memset(bytes, 0, size);
  bytes[0] = 1;
  bytes[1] = static_cast<std::uint8_t>(candidate.address_type);
  bytes[2] = candidate.advertisement.name_size;
  std::memcpy(bytes + 3, candidate.address, 6);
  std::memcpy(bytes + 9, candidate.advertisement.name, bytes[2]);
  const auto crc = sandisolar::crc16_modbus(bytes, size - 2);
  bytes[42] = static_cast<std::uint8_t>(crc);
  bytes[43] = static_cast<std::uint8_t>(crc >> 8);
  return true;
}
inline bool decode(const std::uint8_t* bytes, std::size_t length,
                   sandisolar::BleScanCandidate& output) {
  if (!bytes || length != size || bytes[0] != 1 || bytes[1] > 1 ||
      !bytes[2] || bytes[2] > sandisolar::kBleLoggerIdentityMaxSize) return false;
  const auto crc = sandisolar::crc16_modbus(bytes, size - 2);
  if (bytes[42] != (crc & 255) || bytes[43] != (crc >> 8)) return false;
  for (unsigned i = 9 + bytes[2]; i < 42; ++i) if (bytes[i]) return false;
  sandisolar::BleScanCandidate candidate{};
  candidate.address_type = static_cast<sandisolar::BleAddressType>(bytes[1]);
  std::memcpy(candidate.address, bytes + 3, 6);
  candidate.advertisement.name_size = bytes[2];
  std::memcpy(candidate.advertisement.name, bytes + 9, bytes[2]);
  if (!valid(candidate)) return false;
  output = candidate;  // failed decode leaves caller state untouched
  return true;
}
inline bool matches(const sandisolar::BleScanCandidate& candidate, const char* exact_name) {
  return exact_name && (!*exact_name ||
      std::strcmp(candidate.advertisement.name, exact_name) == 0);
}
}  // namespace saved_logger
