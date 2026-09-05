#include "haipower/register_value.h"

namespace haipower {
Result<RawRegisterValue> decode_register_value(const RegisterBlockView& block,
                                              std::uint16_t address,
                                              Encoding encoding) {
  if ((block.table != RegisterTable::Input && block.table != RegisterTable::Holding) ||
      (encoding != Encoding::U16 && encoding != Encoding::S16 &&
       encoding != Encoding::U32BigEndian && encoding != Encoding::S32BigEndian) ||
      block.count > 65536UL - block.start) return {Error::InvalidArgument,{}};
  const std::uint8_t count = (encoding == Encoding::U16 || encoding == Encoding::S16) ? 1 : 2;
  if (!block.values || address < block.start) return {Error::FieldOutOfRange,{}};
  const std::size_t offset = address - block.start;
  if (offset > block.count || count > block.count - offset) return {Error::FieldOutOfRange,{}};
  RawRegisterValue result{block.table,address,encoding,{block.values[offset],0},count,0};
  if (count == 1) {
    result.value = result.words[0];
    if (encoding == Encoding::S16 && result.words[0] > 0x7fff)
      result.value -= 65536;
  } else {
    result.words[1] = block.values[offset+1];
    const std::uint32_t combined = (static_cast<std::uint32_t>(result.words[0]) << 16) | result.words[1];
    result.value = combined;
    if (encoding == Encoding::S32BigEndian && combined > 0x7fffffffUL)
      result.value -= 4294967296LL;
  }
  return {Error::None,result};
}
}  // namespace haipower
