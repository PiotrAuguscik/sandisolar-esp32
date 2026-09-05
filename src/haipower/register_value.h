#pragma once
#include "haipower/types.h"

namespace haipower {
// Owned wire evidence plus an explicit integer interpretation, not a named
// physical quantity. No scale, model inference, validity/freshness or I/O.
struct RawRegisterValue {
  RegisterTable table;
  std::uint16_t address;
  Encoding encoding;
  std::uint16_t words[2];
  std::uint8_t word_count;
  std::int64_t value;
};

// Decode/reinterpret an already validated block. Unknown addresses are allowed;
// unknown encodings, truncated values and invalid 16-bit ranges are rejected.
Result<RawRegisterValue> decode_register_value(const RegisterBlockView& block,
                                              std::uint16_t address,
                                              Encoding encoding);
}  // namespace haipower
