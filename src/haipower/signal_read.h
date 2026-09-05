#pragma once

#include "haipower/ble_session.h"
#include "haipower/profile.h"

namespace haipower {

// Named convenience layer above the profile and transport. Error::None means
// accepted, NOT a measurement. Keep polling/taking the existing session.
// No extra state, retries, caching or automatic connection. The normal session
// limits, MTU checks and lifecycle apply. Planning errors never initiate I/O.
// The pointer/count form requires an explicit slave to avoid ambiguity with
// the array overload's optional slave argument (which defaults to 1).
Error read_signals(BleReadSession& session, const ProfileView& profile,
                   const SignalId* signals, std::size_t count,
                   std::uint32_t now_ms, std::uint8_t slave);
Error read_signal(BleReadSession& session, const ProfileView& profile,
                  SignalId signal, std::uint32_t now_ms, std::uint8_t slave = 1);
template <std::size_t N>
Error read_signals(BleReadSession& session, const ProfileView& profile,
                   const SignalId (&signals)[N], std::uint32_t now_ms,
                   std::uint8_t slave = 1) {
  return read_signals(session, profile, signals, N, now_ms, slave);
}

// Decode an owned result returned by session.take(). Propagates a failed read
// instead of interpreting its cleared registers as zero measurements. Retain
// the same profile for requesting/decoding; no profile is inferred from BLE.
// This overload does not revalidate arbitrary externally constructed packets
// or infer freshness. Returned Reading owns its value and quality metadata.
Result<Reading> decode_signal(const ProfileView& profile, SignalId signal,
                              const BleReadResult& result);

}  // namespace haipower
