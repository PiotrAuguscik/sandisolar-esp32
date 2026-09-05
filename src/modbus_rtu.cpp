#include "haipower/modbus_rtu.h"

#include "haipower/crc16_modbus.h"

namespace haipower {
namespace {

constexpr std::uint8_t function_code(RegisterTable table) {
  return table == RegisterTable::Holding ? 0x03U : 0x04U;
}

bool has_valid_crc(const std::uint8_t* frame, std::size_t frame_size) {
  if (frame_size < 4) {
    return false;
  }
  const std::uint16_t expected = crc16_modbus(frame, frame_size - 2);
  const std::uint16_t received = static_cast<std::uint16_t>(
      frame[frame_size - 2] | (static_cast<std::uint16_t>(frame[frame_size - 1]) << 8U));
  return expected == received;
}

}  // namespace

Result<std::size_t> build_read_request(std::uint8_t slave,
                                       RegisterTable table,
                                       std::uint16_t start,
                                       std::uint16_t count,
                                       std::uint8_t* output,
                                       std::size_t output_capacity) {
  if (slave == 0 || slave == 0xff || count == 0 || count > kMaxReadRegisters ||
      static_cast<std::uint32_t>(start) + count > 0x10000UL || output == nullptr) {
    return {Error::InvalidArgument, 0};
  }
  if (output_capacity < kReadRequestSize) {
    return {Error::BufferTooSmall, 0};
  }

  output[0] = slave;
  output[1] = function_code(table);
  output[2] = static_cast<std::uint8_t>(start >> 8U);
  output[3] = static_cast<std::uint8_t>(start);
  output[4] = static_cast<std::uint8_t>(count >> 8U);
  output[5] = static_cast<std::uint8_t>(count);
  const std::uint16_t crc = crc16_modbus(output, 6);
  output[6] = static_cast<std::uint8_t>(crc);
  output[7] = static_cast<std::uint8_t>(crc >> 8U);
  return {Error::None, kReadRequestSize};
}

Result<std::size_t> parse_read_response(const std::uint8_t* frame,
                                        std::size_t frame_size,
                                        std::uint8_t expected_slave,
                                        RegisterTable expected_table,
                                        std::uint16_t expected_count,
                                        std::uint16_t* output,
                                        std::size_t output_capacity) {
  if (frame == nullptr || output == nullptr || expected_slave == 0 ||
      expected_count == 0 || expected_count > kMaxReadRegisters) {
    return {Error::InvalidArgument, 0};
  }
  if (frame_size < 5) {
    return {Error::BadLength, 0};
  }
  if (!has_valid_crc(frame, frame_size)) {
    return {Error::BadCrc, 0};
  }
  if (frame[0] != expected_slave) {
    return {Error::WrongSlave, 0};
  }

  const std::uint8_t expected_function = function_code(expected_table);
  if (frame[1] == static_cast<std::uint8_t>(expected_function | 0x80U)) {
    if (frame_size != 5) {
      return {Error::BadLength, 0};
    }
    return {Error::DeviceException, 0};
  }
  if (frame[1] != expected_function) {
    return {Error::WrongFunction, 0};
  }

  const std::size_t byte_count = frame[2];
  if ((byte_count & 1U) != 0U || byte_count != expected_count * 2U ||
      frame_size != byte_count + 5U) {
    return {Error::BadLength, 0};
  }
  if (output_capacity < expected_count) {
    return {Error::BufferTooSmall, 0};
  }

  for (std::size_t index = 0; index < expected_count; ++index) {
    output[index] = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[3 + index * 2]) << 8U) |
        frame[4 + index * 2]);
  }
  return {Error::None, expected_count};
}

}  // namespace haipower
