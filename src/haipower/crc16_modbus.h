#pragma once

#include <cstddef>
#include <cstdint>

namespace haipower {

std::uint16_t crc16_modbus(const std::uint8_t* data, std::size_t size);

}  // namespace haipower
