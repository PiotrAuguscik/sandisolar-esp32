#include "haipower/signal_read.h"

namespace haipower {

Error read_signals(BleReadSession& session, const ProfileView& profile,
                   const SignalId* signals, std::size_t count,
                   std::uint32_t now_ms, std::uint8_t slave) {
  const auto range = signal_read_range(profile, signals, count);
  if (!range) return range.error;
  return session.read({slave, range.value.table, range.value.start, range.value.count}, now_ms);
}

Error read_signal(BleReadSession& session, const ProfileView& profile,
                  SignalId signal, std::uint32_t now_ms, std::uint8_t slave) {
  return read_signals(session, profile, &signal, 1, now_ms, slave);
}

Result<Reading> decode_signal(const ProfileView& profile, SignalId signal,
                              const BleReadResult& result) {
  if (result.error != Error::None) return {result.error, {}};
  if (result.request.count == 0 || result.request.count > kBlePageRegisterCount)
    return {Error::InvalidArgument, {}};
  return decode_signal(profile, signal, RegisterBlockView{result.request.table,
      result.request.start, result.registers, result.request.count});
}

}  // namespace haipower
