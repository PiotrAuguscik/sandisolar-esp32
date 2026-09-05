#pragma once

#include <cstddef>

#include "haipower/types.h"

namespace haipower {

struct FieldSpec {
  SignalId signal;
  RegisterTable table;
  std::uint16_t address;
  Encoding encoding;
  std::int8_t decimal_scale;
  Unit unit;
  Quality quality;
};

struct ProfileView {
  const char* id;
  const FieldSpec* fields;
  std::size_t field_count;
};

const ProfileView& aohai_modbus_v214_profile();
const FieldSpec* find_field(const ProfileView& profile, SignalId signal);

struct RegisterReadRange {
  RegisterTable table;
  std::uint16_t start;
  std::uint16_t count;
};

// Pure planning: smallest contiguous range covering every requested signal,
// including both words of 32-bit fields. No I/O, allocation or profile guess.
// Empty/invalid input -> InvalidArgument; missing signal -> UnsupportedSignal;
// mixed tables or a span exceeding 125 registers -> FieldOutOfRange.
// Gaps between selected fields are read too. Split distant groups explicitly.
Result<RegisterReadRange> signal_read_range(const ProfileView& profile,
                                          const SignalId* signals,
                                          std::size_t count);
Result<RegisterReadRange> signal_read_range(const ProfileView& profile,
                                          SignalId signal);
template <std::size_t N>
Result<RegisterReadRange> signal_read_range(const ProfileView& profile,
                                          const SignalId (&signals)[N]) {
  return signal_read_range(profile, signals, N);
}

// Pure decoding of an already validated register block. Does not perform I/O
// or infer freshness. Inverter fault/warning signals return numeric codes;
// Result.error describes decoding failure, not the inverter's operating state.
Result<Reading> decode_signal(const ProfileView& profile,
                              SignalId signal,
                              const RegisterBlockView& block);

}  // namespace haipower
