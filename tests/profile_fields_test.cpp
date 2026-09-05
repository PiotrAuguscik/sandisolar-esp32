#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "haipower.h"

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { \
  std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while (false)

using haipower::SignalId;
using haipower::Unit;
using haipower::RegisterTable;

void test_existing_ids_are_stable() {
  const SignalId original[] = {
      SignalId::InverterStatus, SignalId::GridVoltageR, SignalId::GridCurrentR,
      SignalId::GridFrequency, SignalId::Pv1Voltage, SignalId::Pv1Current,
      SignalId::PvTotalPower, SignalId::ActiveOutputPower, SignalId::BatteryVoltage,
      SignalId::BatterySoc, SignalId::BatteryCurrent, SignalId::BatteryTemperature,
      SignalId::PowerToUser, SignalId::GridExportPower, SignalId::LoadPower,
      SignalId::EpsActivePowerR, SignalId::PvEnergyToday};
  for (std::size_t i = 0; i < sizeof(original)/sizeof(original[0]); ++i)
    CHECK(static_cast<unsigned>(original[i]) == i);
  CHECK(static_cast<unsigned>(Unit::KilowattHour) == 7);
  CHECK(static_cast<unsigned>(Unit::AmpereHour) == 8);
}

void test_new_fields_scales_names_and_bounds() {
  struct Case {
    SignalId signal;
    std::uint16_t address;
    std::size_t words;
    std::uint32_t raw;
    double value;
    std::int8_t scale;
    Unit unit;
    const char* name;
  };
  // Synthetic fixtures, not a redistributed device capture.
  const Case cases[] = {
    {SignalId::InverterFaultCode,32,1,103,103,0,Unit::None,"inverter_fault_code"},
    {SignalId::InverterWarningCode,33,1,103,103,0,Unit::None,"inverter_warning_code"},
    {SignalId::InverterFaultSubcode,34,1,60000,60000,0,Unit::None,"inverter_fault_subcode"},
    {SignalId::InverterWarningSubcode,35,1,65535,65535,0,Unit::None,"inverter_warning_subcode"},
    {SignalId::BatterySoh,152,1,98,98,0,Unit::Percent,"battery_soh"},
    {SignalId::BatteryMaxChargeCurrent,143,1,1255,125.5,-1,Unit::Ampere,"battery_max_charge_current"},
    {SignalId::BatteryMaxDischargeCurrent,144,1,2400,240,-1,Unit::Ampere,"battery_max_discharge_current"},
    {SignalId::BatteryRatedCapacity,145,1,2800,280,-1,Unit::AmpereHour,"battery_rated_capacity"},
    {SignalId::BatteryDischargePower,349,2,123456,12345.6,-1,Unit::Watt,"battery_discharge_power"},
    {SignalId::BatteryChargePower,351,2,21600,2160,-1,Unit::Watt,"battery_charge_power"},
    {SignalId::EpsVoltageR,55,1,2312,231.2,-1,Unit::Volt,"eps_voltage_r"},
    {SignalId::EpsFrequency,54,1,4998,49.98,-2,Unit::Hertz,"eps_frequency"},
    {SignalId::PvEnergyTotal,381,2,123456,12345.6,-1,Unit::KilowattHour,"pv_energy_total"},
  };
  const auto& profile = haipower::aohai_modbus_v214_profile();
  CHECK(profile.field_count == 30);
  CHECK(std::strcmp(haipower::unit_symbol(Unit::AmpereHour), "Ah") == 0);
  for (const auto& c : cases) {
    const std::uint16_t words[] = {
        static_cast<std::uint16_t>(c.words == 1 ? c.raw : c.raw >> 16),
        static_cast<std::uint16_t>(c.raw)};
    const haipower::RegisterBlockView block = {RegisterTable::Input,c.address,words,c.words};
    const auto result = haipower::decode_signal(profile,c.signal,block);
    CHECK(result);
    CHECK(result.value.signal == c.signal);
    CHECK(result.value.raw == c.raw);
    CHECK(result.value.decimal_scale == c.scale);
    CHECK(result.value.unit == c.unit);
    CHECK(std::abs(haipower::scaled_value(result.value) - c.value) < 0.000001);
    CHECK(std::strcmp(haipower::signal_name(c.signal),c.name) == 0);
    const auto* field = haipower::find_field(profile,c.signal);
    CHECK(field != nullptr);
    if (field) CHECK(field->address == c.address && field->table == RegisterTable::Input);
    CHECK(haipower::decode_signal(profile,c.signal,
        {RegisterTable::Holding,c.address,words,c.words}).error == haipower::Error::FieldOutOfRange);
    CHECK(haipower::decode_signal(profile,c.signal,
        {RegisterTable::Input,c.address,words,c.words-1}).error == haipower::Error::FieldOutOfRange);
    CHECK(haipower::decode_signal(profile,c.signal,
        {RegisterTable::Input,c.address,nullptr,c.words}).error == haipower::Error::FieldOutOfRange);
    CHECK(haipower::decode_signal(profile,c.signal,
        {RegisterTable::Input,static_cast<std::uint16_t>(c.address+1),words,c.words}).error ==
        haipower::Error::FieldOutOfRange);
  }
  for (std::size_t i = 0; i < profile.field_count; ++i) {
    CHECK(std::strcmp(haipower::signal_name(profile.fields[i].signal),"unknown") != 0);
    for (std::size_t j = i+1; j < profile.field_count; ++j)
      CHECK(profile.fields[i].signal != profile.fields[j].signal);
  }
  CHECK(haipower::decode_signal(profile,static_cast<SignalId>(65535),
      {RegisterTable::Input,0,nullptr,0}).error == haipower::Error::UnsupportedSignal);
}

void test_codes_are_data_not_operation_errors() {
  const auto& profile = haipower::aohai_modbus_v214_profile();
  const SignalId codes[] = {SignalId::InverterFaultCode,SignalId::InverterWarningCode,
                           SignalId::InverterFaultSubcode,SignalId::InverterWarningSubcode};
  const std::uint16_t values[] = {103,103,0,65535};
  const haipower::RegisterBlockView block = {RegisterTable::Input,32,values,4};
  for (std::size_t i = 0; i < 4; ++i) {
    const auto r = haipower::decode_signal(profile,codes[i],block);
    CHECK(r.error == haipower::Error::None);
    CHECK(r.value.signal == codes[i]);
    CHECK(r.value.raw == values[i]);
    CHECK(r.value.decimal_scale == 0 && r.value.unit == Unit::None);
  }
  // The same numeric code in two categories remains two distinct readings.
  const auto missing = haipower::decode_signal(profile,SignalId::InverterWarningCode,
      {RegisterTable::Input,32,values,1});
  CHECK(!missing);
  CHECK(missing.error == haipower::Error::FieldOutOfRange);
}

void test_charge_discharge_are_independent_measurements() {
  const auto& profile = haipower::aohai_modbus_v214_profile();
  std::uint16_t words[] = {0,300,0,0};  // discharge 30 W, charge 0 W
  const haipower::RegisterBlockView block = {RegisterTable::Input,349,words,4};
  const auto discharge = haipower::decode_signal(profile,SignalId::BatteryDischargePower,block);
  const auto charge = haipower::decode_signal(profile,SignalId::BatteryChargePower,block);
  CHECK(discharge && charge);
  CHECK(haipower::scaled_value(discharge.value) == 30);
  CHECK(haipower::scaled_value(charge.value) == 0);
  words[1] = 0; words[3] = 21600;  // later PV top-up, 2160 W
  const auto later_discharge = haipower::decode_signal(profile,SignalId::BatteryDischargePower,block);
  const auto later_charge = haipower::decode_signal(profile,SignalId::BatteryChargePower,block);
  CHECK(later_discharge && later_charge);
  CHECK(later_discharge.value.raw == 0);
  CHECK(haipower::scaled_value(later_charge.value) == 2160);
  CHECK(discharge.value.raw == 300);  // Returned readings own their values.
  // Zero power cannot be promoted into a parser failure or a missing reading.
  words[3] = 0;
  CHECK(haipower::decode_signal(profile,SignalId::BatteryChargePower,block));
}
}  // namespace

int main() {
  test_existing_ids_are_stable();
  test_new_fields_scales_names_and_bounds();
  test_codes_are_data_not_operation_errors();
  test_charge_discharge_are_independent_measurements();
  if (failures) return 1;
  std::cout << "All numeric-code and telemetry profile tests passed\n";
  return 0;
}
