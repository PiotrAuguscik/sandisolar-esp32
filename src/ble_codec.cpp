#include "haipower/ble_codec.h"

#include <cstring>

#include "haipower/crc16_modbus.h"
#include "haipower/modbus_rtu.h"

namespace haipower {

const std::uint8_t kHaiPowerAesKey[16] = {
    'a', 'o', 'h', 'a', 'i', '0', '3', '_',
    'a', 'e', 's', '1', '6', 'k', 'e', 'y'};
const std::uint8_t kHaiPowerAesIv[16] = {
    'a', 'o', 'h', 'a', 'i', '0', '3', '_',
    'a', 'e', 's', '1', '6', 'I', 'v', 's'};

namespace {

constexpr std::size_t kRequestPlaintextSize = 48;
// Round up to a CBC block, minimum 32 bytes, with zero fill (not PKCS#7).
std::size_t app_buffer_size(std::size_t size) {
  const std::size_t rounded = ((size + 15U) / 16U) * 16U;
  return rounded < 32U ? 32U : rounded;
}

std::uint16_t read_be16(const std::uint8_t* input) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(input[0]) << 8U) | input[1]);
}

void write_be16(std::uint16_t value, std::uint8_t* output) {
  output[0] = static_cast<std::uint8_t>(value >> 8U);
  output[1] = static_cast<std::uint8_t>(value);
}

}  // namespace

bool valid_ble_logger_identity(BleLoggerIdentity identity) {
  if (identity.data == nullptr || identity.size == 0 ||
      identity.size > kBleLoggerIdentityMaxSize) return false;
  for (std::size_t i = 0; i < identity.size; ++i) {
    const auto byte = static_cast<unsigned char>(identity.data[i]);
    if (byte < 0x20 || byte > 0x7e) return false;
  }
  return true;
}

Result<std::size_t> build_ble_page_read(
    BleLoggerIdentity identity,
    std::uint8_t slave,
    RegisterTable table,
    std::uint16_t start,
    std::uint16_t count,
    std::uint8_t correlation,
    const Aes128Cbc& crypto,
    std::uint8_t* output,
    std::size_t output_capacity) {
  if (!valid_ble_logger_identity(identity) || slave == 0 || slave > 247 ||
      (table != RegisterTable::Holding && table != RegisterTable::Input) ||
      output == nullptr || correlation == 0xff ||
      crypto.encrypt == nullptr) {
    return {Error::InvalidArgument, 0};
  }
  const std::size_t plaintext_size = identity.size + 2 + kReadRequestSize;
  const std::size_t ciphertext_size = app_buffer_size(plaintext_size);
  const std::size_t frame_size = 10 + ciphertext_size;
  if (output_capacity < frame_size) {
    return {Error::BufferTooSmall, 0};
  }

  std::uint8_t rtu_request[kReadRequestSize] = {};
  const Result<std::size_t> request = build_read_request(
      slave, table, start, count, rtu_request, sizeof(rtu_request));
  if (!request) {
    return {request.error, 0};
  }

  std::uint8_t plaintext[kRequestPlaintextSize] = {};
  std::memcpy(plaintext, identity.data, identity.size);
  write_be16(kReadRequestSize, plaintext + identity.size);
  // The app appends the normal Modbus low-byte-first CRC inside the RTU.
  std::memcpy(plaintext + identity.size + 2, rtu_request, sizeof(rtu_request));
  // Remaining bytes stay zero; transform full CBC blocks without PKCS#7.

  write_be16(frame_size - 2, output);
  output[2] = 0x00;
  output[3] = 0x01;
  write_be16(ciphertext_size + 2, output + 4);
  output[6] = correlation;
  output[7] = kBleMarker;
  if (!crypto.encrypt(kHaiPowerAesKey, kHaiPowerAesIv, plaintext,
                      ciphertext_size, output + 8, crypto.context)) {
    return {Error::CryptoFailure, 0};
  }

  // Unlike the inner RTU, the outer CRC is big-endian and INCLUDES length.
  const std::uint16_t outer_crc = crc16_modbus(output, frame_size - 2);
  write_be16(outer_crc, output + frame_size - 2);
  return {Error::None, frame_size};
}

Result<std::size_t> decrypt_ble_notification(
    const std::uint8_t* frame, std::size_t frame_size,
    const Aes128Cbc& crypto, std::uint8_t* plaintext,
    std::size_t plaintext_capacity) {
  if (frame == nullptr || plaintext == nullptr || crypto.decrypt == nullptr)
    return {Error::InvalidArgument, 0};
  if (frame_size <= 8 || frame_size > 8 + kBleNotificationPlaintextCapacity)
    return {Error::BadLength, 0};
  // Skip the eight-byte envelope and zero-extend the remaining diagnostic
  // bytes, including any opaque trailer. This is NOT validated telemetry.
  const std::size_t source_size = frame_size - 8;
  const std::size_t ciphertext_size = app_buffer_size(source_size);
  if (plaintext_capacity < ciphertext_size) return {Error::BufferTooSmall, 0};
  std::uint8_t ciphertext[kBleNotificationPlaintextCapacity]{};
  std::memcpy(ciphertext, frame + 8, source_size);
  if (!crypto.decrypt(kHaiPowerAesKey, kHaiPowerAesIv, ciphertext,
                      ciphertext_size, plaintext, crypto.context))
    return {Error::CryptoFailure, 0};
  return {Error::None, ciphertext_size};
}

Result<std::size_t> parse_ble_read_plaintext(
    const std::uint8_t* plaintext, std::size_t plaintext_size,
    BleLoggerIdentity identity, std::uint8_t slave, RegisterTable table,
    std::uint16_t expected_count, std::uint16_t* registers,
    std::size_t register_capacity) {
  if (plaintext == nullptr || !valid_ble_logger_identity(identity) ||
      slave == 0 || slave > 247 ||
      (table != RegisterTable::Holding && table != RegisterTable::Input))
    return {Error::InvalidArgument, 0};
  if (plaintext_size < identity.size + 2) return {Error::BadLength, 0};
  if (std::memcmp(plaintext, identity.data, identity.size) != 0)
    return {Error::WrongDeviceIdentity, 0};
  const std::size_t rtu_size = read_be16(plaintext + identity.size);
  const std::size_t content_size = identity.size + 2 + rtu_size;
  if (plaintext_size != content_size &&
      plaintext_size != app_buffer_size(content_size))
    return {Error::BadLength, 0};
  if (content_size > plaintext_size) return {Error::BadLength, 0};
  for (std::size_t i = content_size; i < plaintext_size; ++i)
    if (plaintext[i] != 0) return {Error::BadPadding, 0};
  return parse_read_response(plaintext + identity.size + 2, rtu_size,
                             slave, table, expected_count, registers,
                             register_capacity);
}

Result<std::size_t> parse_ble_read_frame(
    const std::uint8_t* frame, std::size_t frame_size,
    BleLoggerIdentity identity, std::uint8_t slave, RegisterTable table,
    std::uint16_t expected_count, std::uint8_t expected_correlation,
    const Aes128Cbc& crypto, std::uint16_t* registers,
    std::size_t register_capacity) {
  if (frame == nullptr || crypto.decrypt == nullptr || registers == nullptr ||
      !valid_ble_logger_identity(identity) || slave == 0 || slave > 247 ||
      (table != RegisterTable::Input && table != RegisterTable::Holding) ||
      expected_count == 0 || expected_count > kMaxReadRegisters || expected_correlation == 0xff)
    return {Error::InvalidArgument, 0};
  if (register_capacity < expected_count) return {Error::BufferTooSmall, 0};
  if (frame_size < 42 || frame_size > 10 + kBleNotificationPlaintextCapacity ||
      (frame_size - 10) % 16 != 0 || read_be16(frame) != frame_size - 2 ||
      read_be16(frame + 4) != frame_size - 8)
    return {Error::BadLength, 0};
  if (frame[2] != 0 || frame[3] != 1 || frame[7] != kBleMarker)
    return {Error::BadMarker, 0};
  if (crc16_modbus(frame, frame_size - 2) != read_be16(frame + frame_size - 2))
    return {Error::BadCrc, 0};
  if (frame[6] != expected_correlation) return {Error::WrongCorrelation, 0};
  std::uint8_t plaintext[kBleNotificationPlaintextCapacity]{};
  const std::size_t ciphertext_size = frame_size - 10;
  if (!crypto.decrypt(kHaiPowerAesKey, kHaiPowerAesIv, frame + 8,
                      ciphertext_size, plaintext, crypto.context))
    return {Error::CryptoFailure, 0};
  return parse_ble_read_plaintext(plaintext, ciphertext_size, identity, slave,
                                  table, expected_count, registers, register_capacity);
}

}  // namespace haipower
