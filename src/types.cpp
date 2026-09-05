#include "haipower/types.h"

namespace haipower {

const char* error_name(Error error) {
  switch (error) {
    case Error::None: return "none";
    case Error::InvalidArgument: return "invalid_argument";
    case Error::BufferTooSmall: return "buffer_too_small";
    case Error::BadCrc: return "bad_crc";
    case Error::BadLength: return "bad_length";
    case Error::BadMarker: return "bad_marker";
    case Error::BadPadding: return "bad_padding";
    case Error::CryptoFailure: return "crypto_failure";
    case Error::WrongSlave: return "wrong_slave";
    case Error::WrongFunction: return "wrong_function";
    case Error::DeviceException: return "device_exception";
    case Error::UnsupportedSignal: return "unsupported_signal";
    case Error::FieldOutOfRange: return "field_out_of_range";
    case Error::WrongDeviceIdentity: return "wrong_device_identity";
    case Error::WrongCorrelation: return "wrong_correlation";
    case Error::Busy: return "busy";
    case Error::NotConnected: return "not_connected";
    case Error::Timeout: return "timeout";
    case Error::TransportFailure: return "transport_failure";
    case Error::TransportFault: return "transport_fault";
    case Error::MtuTooSmall: return "mtu_too_small";
    case Error::CorrelationExhausted: return "correlation_exhausted";
    case Error::NotFound: return "not_found";
    case Error::AmbiguousDevice: return "ambiguous_device";
    case Error::IncompleteScan: return "incomplete_scan";
  }
  return "unknown";
}

const char* signal_name(SignalId signal) {
  switch (signal) {
    case SignalId::InverterStatus: return "inverter_status";
    case SignalId::GridVoltageR: return "grid_voltage_r";
    case SignalId::GridCurrentR: return "grid_current_r";
    case SignalId::GridFrequency: return "grid_frequency";
    case SignalId::Pv1Voltage: return "pv1_voltage";
    case SignalId::Pv1Current: return "pv1_current";
    case SignalId::PvTotalPower: return "pv_total_power";
    case SignalId::ActiveOutputPower: return "active_output_power";
    case SignalId::BatteryVoltage: return "battery_voltage";
    case SignalId::BatterySoc: return "battery_soc";
    case SignalId::BatteryCurrent: return "battery_current";
    case SignalId::BatteryTemperature: return "battery_temperature";
    case SignalId::PowerToUser: return "power_to_user";
    case SignalId::GridExportPower: return "grid_export_power";
    case SignalId::LoadPower: return "load_power";
    case SignalId::EpsActivePowerR: return "eps_active_power_r";
    case SignalId::PvEnergyToday: return "pv_energy_today";
    case SignalId::InverterFaultCode: return "inverter_fault_code";
    case SignalId::InverterWarningCode: return "inverter_warning_code";
    case SignalId::InverterFaultSubcode: return "inverter_fault_subcode";
    case SignalId::InverterWarningSubcode: return "inverter_warning_subcode";
    case SignalId::BatterySoh: return "battery_soh";
    case SignalId::BatteryMaxChargeCurrent: return "battery_max_charge_current";
    case SignalId::BatteryMaxDischargeCurrent: return "battery_max_discharge_current";
    case SignalId::BatteryRatedCapacity: return "battery_rated_capacity";
    case SignalId::BatteryDischargePower: return "battery_discharge_power";
    case SignalId::BatteryChargePower: return "battery_charge_power";
    case SignalId::EpsVoltageR: return "eps_voltage_r";
    case SignalId::EpsFrequency: return "eps_frequency";
    case SignalId::PvEnergyTotal: return "pv_energy_total";
  }
  return "unknown";
}

const char* unit_symbol(Unit unit) {
  switch (unit) {
    case Unit::None: return "";
    case Unit::Percent: return "%";
    case Unit::Volt: return "V";
    case Unit::Ampere: return "A";
    case Unit::Watt: return "W";
    case Unit::Hertz: return "Hz";
    case Unit::Celsius: return "C";
    case Unit::KilowattHour: return "kWh";
    case Unit::AmpereHour: return "Ah";
    case Unit::Second: return "s";
    case Unit::VoltAmpere: return "VA";
    case Unit::Var: return "var";
    case Unit::RevolutionsPerMinute: return "rpm";
    case Unit::Ohm: return "ohm";
  }
  return "";
}

}  // namespace haipower
