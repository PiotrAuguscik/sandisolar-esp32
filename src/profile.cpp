#include "haipower/profile.h"
#include "haipower/modbus_rtu.h"
#include "haipower/register_value.h"

#include <cstdint>

namespace haipower {
namespace {

constexpr FieldSpec kFields[] = {
    {SignalId::InverterStatus, RegisterTable::Input, 0, Encoding::U16, 0,
     Unit::None, Quality::Good},
    {SignalId::GridVoltageR, RegisterTable::Input, 42, Encoding::U16, -1,
     Unit::Volt, Quality::Good},
    {SignalId::GridCurrentR, RegisterTable::Input, 43, Encoding::S16, -1,
     Unit::Ampere, Quality::Good},
    {SignalId::GridFrequency, RegisterTable::Input, 51, Encoding::U16, -2,
     Unit::Hertz, Quality::Good},
    {SignalId::Pv1Voltage, RegisterTable::Input, 64, Encoding::U16, -1,
     Unit::Volt, Quality::Good},
    {SignalId::Pv1Current, RegisterTable::Input, 65, Encoding::U16, -1,
     Unit::Ampere, Quality::Good},
    {SignalId::BatteryVoltage, RegisterTable::Input, 127, Encoding::U16, -1,
     Unit::Volt, Quality::Good},
    {SignalId::BatterySoc, RegisterTable::Input, 128, Encoding::U16, 0,
     Unit::Percent, Quality::UnverifiedScale},
    {SignalId::BatteryCurrent, RegisterTable::Input, 141, Encoding::S16, -2,
     Unit::Ampere, Quality::Good},
    {SignalId::BatteryTemperature, RegisterTable::Input, 142, Encoding::S16, -1,
     Unit::Celsius, Quality::Good},
    {SignalId::PvTotalPower, RegisterTable::Input, 250, Encoding::U32BigEndian,
     -1, Unit::Watt, Quality::Good},
    {SignalId::ActiveOutputPower, RegisterTable::Input, 286,
     Encoding::S32BigEndian, -1, Unit::Watt, Quality::Good},
    {SignalId::PowerToUser, RegisterTable::Input, 320, Encoding::U32BigEndian,
     -1, Unit::Watt, Quality::Good},
    {SignalId::GridExportPower, RegisterTable::Input, 328,
     Encoding::U32BigEndian, -1, Unit::Watt, Quality::Good},
    {SignalId::LoadPower, RegisterTable::Input, 330, Encoding::U32BigEndian, -1,
     Unit::Watt, Quality::Good},
    {SignalId::EpsActivePowerR, RegisterTable::Input, 334,
     Encoding::U32BigEndian, -1, Unit::Watt, Quality::Good},
    {SignalId::PvEnergyToday, RegisterTable::Input, 379,
     Encoding::U32BigEndian, -1, Unit::KilowattHour, Quality::Good},
    // Keep fault/warning and main/subcode namespaces separate. Unknown codes
    // remain exact U16 values; their meaning belongs to model documentation.
    {SignalId::InverterFaultCode, RegisterTable::Input, 32, Encoding::U16, 0,
     Unit::None, Quality::Good},
    {SignalId::InverterWarningCode, RegisterTable::Input, 33, Encoding::U16, 0,
     Unit::None, Quality::Good},
    {SignalId::InverterFaultSubcode, RegisterTable::Input, 34, Encoding::U16, 0,
     Unit::None, Quality::Good},
    {SignalId::InverterWarningSubcode, RegisterTable::Input, 35, Encoding::U16, 0,
     Unit::None, Quality::Good},
    {SignalId::BatterySoh, RegisterTable::Input, 152, Encoding::U16, 0,
     Unit::Percent, Quality::Good},
    {SignalId::BatteryMaxChargeCurrent, RegisterTable::Input, 143,
     Encoding::U16, -1, Unit::Ampere, Quality::Good},
    {SignalId::BatteryMaxDischargeCurrent, RegisterTable::Input, 144,
     Encoding::U16, -1, Unit::Ampere, Quality::Good},
    {SignalId::BatteryRatedCapacity, RegisterTable::Input, 145,
     Encoding::U16, -1, Unit::AmpereHour, Quality::Good},
    {SignalId::BatteryDischargePower, RegisterTable::Input, 349,
     Encoding::U32BigEndian, -1, Unit::Watt, Quality::Good},
    {SignalId::BatteryChargePower, RegisterTable::Input, 351,
     Encoding::U32BigEndian, -1, Unit::Watt, Quality::Good},
    {SignalId::EpsVoltageR, RegisterTable::Input, 55, Encoding::U16, -1,
     Unit::Volt, Quality::Good},
    {SignalId::EpsFrequency, RegisterTable::Input, 54, Encoding::U16, -2,
     Unit::Hertz, Quality::Good},
    {SignalId::PvEnergyTotal, RegisterTable::Input, 381,
     Encoding::U32BigEndian, -1, Unit::KilowattHour, Quality::Good},
};

}  // namespace

const ProfileView& aohai_modbus_v214_profile() {
  static const ProfileView profile = {
      "aohai-modbus-v2.14", kFields, sizeof(kFields) / sizeof(kFields[0])};
  return profile;
}

const FieldSpec* find_field(const ProfileView& profile, SignalId signal) {
  if (!profile.fields) return nullptr;
  for (std::size_t index = 0; index < profile.field_count; ++index) {
    if (profile.fields[index].signal == signal) {
      return &profile.fields[index];
    }
  }
  return nullptr;
}

Result<RegisterReadRange> signal_read_range(const ProfileView& profile,
                                          const SignalId* signals,
                                          std::size_t count) {
  if (!signals || count == 0) return {Error::InvalidArgument, {}};
  RegisterTable table = RegisterTable::Input;
  std::uint32_t begin = 65536UL;
  std::uint32_t end = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto* field = find_field(profile, signals[i]);
    if (!field) return {Error::UnsupportedSignal, {}};
    if (field->table != RegisterTable::Input && field->table != RegisterTable::Holding)
      return {Error::InvalidArgument, {}};
    std::uint32_t width = 0;
    switch (field->encoding) {
      case Encoding::U16: case Encoding::S16: width = 1; break;
      case Encoding::U32BigEndian: case Encoding::S32BigEndian: width = 2; break;
      default: return {Error::InvalidArgument, {}};
    }
    const std::uint32_t field_end = static_cast<std::uint32_t>(field->address) + width;
    if (field_end > 65536UL) return {Error::InvalidArgument, {}};
    if (i == 0) table = field->table;
    if (field->table != table) return {Error::FieldOutOfRange, {}};
    if (field->address < begin) begin = field->address;
    if (field_end > end) end = field_end;
  }
  if (end - begin > kMaxReadRegisters) return {Error::FieldOutOfRange, {}};
  return {Error::None, {table, static_cast<std::uint16_t>(begin),
                       static_cast<std::uint16_t>(end - begin)}};
}

Result<RegisterReadRange> signal_read_range(const ProfileView& profile, SignalId signal) {
  return signal_read_range(profile, &signal, 1);
}

Result<Reading> decode_signal(const ProfileView& profile,
                              SignalId signal,
                              const RegisterBlockView& block) {
  const FieldSpec* field = find_field(profile, signal);
  if (field == nullptr) {
    return {Error::UnsupportedSignal, {}};
  }
  if (block.values == nullptr || block.table != field->table ||
      field->address < block.start) {
    return {Error::FieldOutOfRange, {}};
  }

  const auto raw = decode_register_value(block, field->address, field->encoding);
  if (!raw) return {raw.error,{}};

  return {Error::None,
          {field->signal, raw.value.value, field->decimal_scale, field->unit, field->quality}};
}

}  // namespace haipower
