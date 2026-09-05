#include "sandisolar.h"
#include <iostream>

namespace {
using namespace sandisolar;
int failures = 0;
#define CHECK(x) do { if (!(x)) { \
  std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while (false)

void every_named_signal_plans_its_entire_field_and_retains_metadata() {
  const auto& profile = aohai_modbus_v214_profile();
  CHECK(profile.field_count == 30);
  for (std::size_t i = 0; i < profile.field_count; ++i) {
    const auto& field = profile.fields[i];
    const auto range = signal_read_range(profile, field.signal);
    const unsigned width = (field.encoding == Encoding::U16 || field.encoding == Encoding::S16) ? 1 : 2;
    CHECK(range);
    CHECK(range.value.table == field.table && range.value.start == field.address);
    CHECK(range.value.count == width);
    BleReadResult result{};
    result.request = {1, range.value.table, range.value.start, range.value.count};
    result.registers[0] = 0xffff;
    result.registers[1] = 0xff9c;
    const auto decoded = decode_signal(profile, field.signal, result);
    CHECK(decoded);
    CHECK(decoded.value.signal == field.signal && decoded.value.unit == field.unit);
    CHECK(decoded.value.quality == field.quality && decoded.value.decimal_scale == field.decimal_scale);
    const std::int64_t expected = field.encoding == Encoding::S16 ? -1 :
        field.encoding == Encoding::U16 ? 65535 :
        field.encoding == Encoding::S32BigEndian ? -100 : 4294967196LL;
    CHECK(decoded.value.raw == expected);
  }
}

void grouped_reads_are_order_independent_and_include_both_words() {
  const auto& profile = aohai_modbus_v214_profile();
  const SignalId battery[] = {SignalId::BatterySoh, SignalId::BatterySoc,
      SignalId::BatteryVoltage, SignalId::BatteryCurrent, SignalId::BatteryTemperature,
      SignalId::BatteryMaxChargeCurrent, SignalId::BatteryMaxDischargeCurrent,
      SignalId::BatteryRatedCapacity, SignalId::BatterySoc}; // duplicates are harmless
  const auto b = signal_read_range(profile, battery);
  CHECK(b && b.value.start == 127 && b.value.count == 26 && b.value.table == RegisterTable::Input);
  const SignalId power[] = {SignalId::BatteryDischargePower, SignalId::BatteryChargePower,
      SignalId::LoadPower, SignalId::PvTotalPower};
  const auto p = signal_read_range(profile, power);
  CHECK(p && p.value.start == 250 && p.value.count == 103);
  const SignalId reversed[] = {SignalId::PvTotalPower, SignalId::LoadPower,
      SignalId::BatteryChargePower, SignalId::BatteryDischargePower};
  const auto r = signal_read_range(profile, reversed);
  CHECK(r && p.value.table == r.value.table && p.value.start == r.value.start && p.value.count == r.value.count);
  const SignalId energy[] = {SignalId::PvEnergyTotal, SignalId::PvEnergyToday};
  const auto e = signal_read_range(profile, energy);
  CHECK(e && e.value.start == 379 && e.value.count == 4);
}

void invalid_and_unrepresentable_ranges_never_produce_a_partial_plan() {
  const auto& profile = aohai_modbus_v214_profile();
  const SignalId signal = SignalId::BatterySoc;
  CHECK(signal_read_range(profile, nullptr, 1).error == Error::InvalidArgument);
  CHECK(signal_read_range(profile, &signal, 0).error == Error::InvalidArgument);
  CHECK(signal_read_range({"empty", nullptr, 0}, signal).error == Error::UnsupportedSignal);
  const SignalId unknown[] = {signal, static_cast<SignalId>(65535)};
  CHECK(signal_read_range(profile, unknown).error == Error::UnsupportedSignal);
  const SignalId distant[] = {signal, SignalId::PvEnergyTotal};
  CHECK(signal_read_range(profile, distant).error == Error::FieldOutOfRange);

  FieldSpec fields[] = {
      {signal, RegisterTable::Input, 0, Encoding::U16, 0, Unit::Percent, Quality::UnverifiedScale},
      {SignalId::BatteryVoltage, RegisterTable::Input, 124, Encoding::U16, -1, Unit::Volt, Quality::Good}};
  const ProfileView custom = {"boundary-test", fields, 2};
  const SignalId pair[] = {signal, SignalId::BatteryVoltage};
  CHECK(signal_read_range(custom, pair).value.count == 125);
  fields[1].address = 125;
  CHECK(signal_read_range(custom, pair).error == Error::FieldOutOfRange);
  fields[1].address = 124; fields[1].encoding = Encoding::U32BigEndian;
  CHECK(signal_read_range(custom, pair).error == Error::FieldOutOfRange);
  fields[1].address = 123;
  CHECK(signal_read_range(custom, pair).value.count == 125);
  fields[1].table = RegisterTable::Holding;
  CHECK(signal_read_range(custom, pair).error == Error::FieldOutOfRange);
  fields[1].table = static_cast<RegisterTable>(99);
  CHECK(signal_read_range(custom, pair).error == Error::InvalidArgument);
  fields[1].table = RegisterTable::Input;
  fields[1].encoding = static_cast<Encoding>(99);
  CHECK(signal_read_range(custom, pair).error == Error::InvalidArgument);
  fields[0].address = 65535;
  auto last = signal_read_range(custom, signal);
  CHECK(last && last.value.start == 65535 && last.value.count == 1);
  fields[0].encoding = Encoding::S32BigEndian;
  CHECK(signal_read_range(custom, signal).error == Error::InvalidArgument);
  fields[0].address = 65534;
  last = signal_read_range(custom, signal);
  CHECK(last && last.value.start == 65534 && last.value.count == 2);
}

void failed_or_missing_data_is_not_a_zero_and_results_own_values() {
  const auto& profile = aohai_modbus_v214_profile();
  BleReadResult result{};
  result.request = {1, RegisterTable::Input, 128, 1};
  result.registers[0] = 77;
  const auto soc = decode_signal(profile, SignalId::BatterySoc, result);
  CHECK(soc && scaled_value(soc.value) == 77);
  CHECK(soc.value.quality == Quality::UnverifiedScale);
  result.registers[0] = 0;
  CHECK(soc.value.raw == 77); // owned, not a view into the next result
  CHECK(decode_signal(profile, SignalId::BatterySoc, result)); // actual zero is valid
  CHECK(decode_signal(profile, SignalId::BatteryCurrent, result).error == Error::FieldOutOfRange);
  for (auto error : {Error::Timeout, Error::BadCrc, Error::NotConnected,
                     Error::DeviceException, Error::TransportFailure}) {
    result.error = error;
    CHECK(decode_signal(profile, SignalId::BatterySoc, result).error == error);
  }
  result.error = Error::None;
  result.request.count = 126;
  CHECK(decode_signal(profile, SignalId::BatterySoc, result).error == Error::InvalidArgument);
  result.request.count = 65535;
  CHECK(decode_signal(profile, SignalId::BatterySoc, result).error == Error::InvalidArgument);
  result.request.count = 0;
  CHECK(decode_signal(profile, SignalId::BatterySoc, result).error == Error::InvalidArgument);
  result.request = {1, RegisterTable::Holding, 128, 1};
  CHECK(decode_signal(profile, SignalId::BatterySoc, result).error == Error::FieldOutOfRange);
}
}  // namespace

int main() {
  every_named_signal_plans_its_entire_field_and_retains_metadata();
  grouped_reads_are_order_independent_and_include_both_words();
  invalid_and_unrepresentable_ranges_never_produce_a_partial_plan();
  failed_or_missing_data_is_not_a_zero_and_results_own_values();
  std::cout << "Named signal read failures: " << failures << '\n';
  return failures ? 1 : 0;
}
