#include "haipower/ble_session.h"

#include <cstring>
#include <limits>

namespace haipower {
namespace {
constexpr std::uint32_t kConnectTimeout = 30000;
constexpr std::uint32_t kReadTimeout = 10000;
constexpr std::uint32_t kCloseTimeout = 10000;
constexpr std::uint32_t kCommandGap = 1000;
}

BleReadSession::BleReadSession(BleLink& link, Aes128Cbc crypto)
    : link_(link), crypto_(crypto) {}

Error BleReadSession::connect(const BleTarget& target, std::uint32_t now) {
  if (state_ != BleSessionState::Idle || pending_result_) return Error::Busy;
  bool address_nonzero = false;
  for (auto byte : target.address) address_nonzero |= byte != 0;
  if (!valid_ble_logger_identity(target.logger) || !address_nonzero ||
      (target.address_type != BleAddressType::Public &&
       target.address_type != BleAddressType::Random) ||
      !crypto_.encrypt || !crypto_.decrypt) return Error::InvalidArgument;
  if (epoch_ == std::numeric_limits<std::uint32_t>::max()) return Error::TransportFault;
  std::memcpy(logger_, target.logger.data, target.logger.size);
  logger_size_ = target.logger.size;
  BleTarget owned = target;
  owned.logger = {logger_, logger_size_};
  ++epoch_;
  correlation_ = 0;
  last_error_ = last_packet_error_ = Error::None;
  phase_at_ = now;
  state_ = BleSessionState::Connecting;
  const Error error = link_.open(owned, epoch_);
  if (error != Error::None) {
    last_error_ = error;
    state_ = error == Error::TransportFault ? BleSessionState::Fault : BleSessionState::Idle;
  }
  return error;
}

Error BleReadSession::read(BleReadRequest request, std::uint32_t now) {
  if (state_ == BleSessionState::Reading) return Error::Busy;
  if (state_ != BleSessionState::Ready) return Error::NotConnected;
  if (pending_result_ || (gap_active_ && now - gap_at_ < kCommandGap)) return Error::Busy;
  if (correlation_ > 254) return Error::CorrelationExhausted;
  std::uint8_t frame[kBlePageReadFrameSize]{};
  const auto built = build_ble_page_read({logger_, logger_size_}, request.slave,
      request.table, request.start, request.count,
      static_cast<std::uint8_t>(correlation_), crypto_, frame, sizeof(frame));
  if (!built) return built.error;
  // This adapter accepts one complete notification, no speculative fragments.
  const std::size_t plain = logger_size_ + 2 + 5 + 2 * request.count;
  const std::size_t rounded = ((plain + 15) / 16) * 16;
  const std::size_t reply_size = 10 + (rounded < 32 ? 32 : rounded);
  if (mtu_ < 3 || reply_size > static_cast<std::size_t>(mtu_ - 3) ||
      built.value > static_cast<std::size_t>(mtu_ - 3)) return Error::MtuTooSmall;
  result_ = {};
  result_.request = request;
  result_.epoch = epoch_;
  result_.requested_at_ms = now;
  gap_active_ = true;
  gap_at_ = now;
  phase_at_ = now;
  received_ = written_ = false;
  last_error_ = last_packet_error_ = Error::None;
  state_ = BleSessionState::Reading;
  const Error error = link_.write(frame, built.value);
  // Once accepted by this session, even a synchronous adapter failure produces
  // one terminal result. Close fences any ambiguous/late transport completion.
  if (error != Error::None) closing(error, now);
  return Error::None;
}

void BleReadSession::finish(Error error) {
  result_.error = error;
  if (error != Error::None) {
    std::memset(result_.registers, 0, sizeof(result_.registers));
    result_.received_at_ms = 0;
  }
  pending_result_ = true;
  last_error_ = error;
  ++correlation_;
}

void BleReadSession::closing(Error error, std::uint32_t now) {
  if (state_ == BleSessionState::Reading) finish(error);
  last_error_ = error;
  state_ = BleSessionState::Closing;
  phase_at_ = now;
  link_.close();
}

void BleReadSession::disconnect(std::uint32_t now) {
  if (state_ == BleSessionState::Idle || state_ == BleSessionState::Closing ||
      state_ == BleSessionState::Fault) return;
  closing(Error::NotConnected, now);
}

void BleReadSession::poll(std::uint32_t now) {
  // Deadline first: a late/piled-up response must not revive timed-out work.
  if ((state_ == BleSessionState::Connecting && now - phase_at_ >= kConnectTimeout) ||
      (state_ == BleSessionState::Reading && now - phase_at_ >= kReadTimeout))
    closing(Error::Timeout, now);
  if (state_ == BleSessionState::Closing && now - phase_at_ >= kCloseTimeout) {
    state_ = BleSessionState::Fault;
    last_error_ = Error::TransportFault;
  }
  BleLink::Event event{};
  for (unsigned budget = 0; budget < 8 && link_.poll(event); ++budget) {
    if (event.epoch != epoch_ || state_ == BleSessionState::Fault) continue;
    switch (event.type) {
      case BleLink::EventType::Fault:
        if (state_ == BleSessionState::Reading) finish(Error::TransportFault);
        last_error_ = Error::TransportFault;
        state_ = BleSessionState::Fault;
        link_.close();
        break;
      case BleLink::EventType::Closed:
        if (state_ == BleSessionState::Reading) finish(
            event.error == Error::None ? Error::NotConnected : event.error);
        if (event.error != Error::None) last_error_ = event.error;
        state_ = BleSessionState::Idle;
        break;
      case BleLink::EventType::Ready:
        if (state_ == BleSessionState::Connecting) {
          mtu_ = event.mtu;
          state_ = BleSessionState::Ready;
        }
        break;
      case BleLink::EventType::Written:
        if (state_ == BleSessionState::Reading) {
          if (event.error != Error::None) closing(event.error, now);
          else written_ = true;
        }
        break;
      case BleLink::EventType::Notification:
        if (state_ == BleSessionState::Reading && !received_) {
          if (event.size > sizeof(event.bytes)) {
            closing(Error::BadLength, now);
            break;
          }
          const auto parsed = parse_ble_read_frame(event.bytes, event.size,
              {logger_, logger_size_}, result_.request.slave, result_.request.table,
              result_.request.count, static_cast<std::uint8_t>(correlation_),
              crypto_, result_.registers, kBlePageRegisterCount);
          last_packet_error_ = parsed.error;
          if (parsed) {
            received_ = true;
            result_.received_at_ms = event.at_ms;
          }
        }
        break;
    }
    if (state_ == BleSessionState::Reading && written_ && received_) {
      finish(Error::None);
      gap_active_ = true;
      gap_at_ = now;
      state_ = BleSessionState::Ready;
    }
  }
}

bool BleReadSession::take(BleReadResult& result) {
  if (!pending_result_) return false;
  result = result_;
  pending_result_ = false;
  return true;
}
}  // namespace haipower
