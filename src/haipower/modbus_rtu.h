#pragma once

#include <cstddef>
#include <cstdint>

#include "haipower/types.h"

namespace haipower {

constexpr std::size_t kReadRequestSize = 8;
constexpr std::uint16_t kMaxReadRegisters = 125;

Result<std::size_t> build_read_request(std::uint8_t slave,
                                       RegisterTable table,
                                       std::uint16_t start,
                                       std::uint16_t count,
                                       std::uint8_t* output,
                                       std::size_t output_capacity);

Result<std::size_t> parse_read_response(const std::uint8_t* frame,
                                        std::size_t frame_size,
                                        std::uint8_t expected_slave,
                                        RegisterTable expected_table,
                                        std::uint16_t expected_count,
                                        std::uint16_t* output,
                                        std::size_t output_capacity);

}  // namespace haipower
