#include "haipower/crc16_modbus.h"

namespace haipower {

std::uint16_t crc16_modbus(const std::uint8_t* data, std::size_t size) {
  std::uint16_t crc = 0xffff;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) != 0U
                ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xa001U)
                : static_cast<std::uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

}  // namespace haipower
