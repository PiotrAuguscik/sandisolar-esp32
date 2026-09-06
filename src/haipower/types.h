#pragma once

#include <cstddef>
#include <cstdint>

namespace haipower {

enum class Error : std::uint8_t {
  None = 0,
  InvalidArgument,
  BufferTooSmall,
  BadCrc,
  BadLength,
  BadMarker,
  BadPadding,
  CryptoFailure,
  WrongSlave,
  WrongFunction,
  DeviceException,
  UnsupportedSignal,
  FieldOutOfRange,
  WrongDeviceIdentity,
  WrongCorrelation,
  Busy,
  NotConnected,
  Timeout,
  TransportFailure,
  TransportFault,
  MtuTooSmall,
  CorrelationExhausted,
  NotFound,
  AmbiguousDevice,
  IncompleteScan,
  RadioConflict,
};

template <typename T>
struct Result {
  Error error;
  T value;

  constexpr explicit operator bool() const { return error == Error::None; }
};

enum class RegisterTable : std::uint8_t { Holding, Input };

enum class Encoding : std::uint8_t { U16, S16, U32BigEndian, S32BigEndian };

enum class Unit : std::uint8_t {
  None,
  Percent,
  Volt,
  Ampere,
  Watt,
  Hertz,
  Celsius,
  KilowattHour,
  AmpereHour,
  Second,
  VoltAmpere,
  Var,
  RevolutionsPerMinute,
  Ohm,
};

enum class Quality : std::uint8_t { Good, UnverifiedScale };

enum class SignalId : std::uint16_t {
  InverterStatus,
  GridVoltageR,
  GridCurrentR,
  GridFrequency,
  Pv1Voltage,
  Pv1Current,
  PvTotalPower,
  ActiveOutputPower,
  BatteryVoltage,
  BatterySoc,
  BatteryCurrent,
  BatteryTemperature,
  PowerToUser,
  GridExportPower,
  LoadPower,
  EpsActivePowerR,
  PvEnergyToday,
  // Append IDs to preserve the numeric values used by existing consumers.
  // Device codes are unsigned register values, not Error (operation errors).
  InverterFaultCode,
  InverterWarningCode,
  InverterFaultSubcode,
  InverterWarningSubcode,
  BatterySoh,
  BatteryMaxChargeCurrent,
  BatteryMaxDischargeCurrent,
  BatteryRatedCapacity,
  BatteryDischargePower,
  BatteryChargePower,
  EpsVoltageR,
  EpsFrequency,
  PvEnergyTotal,
};

struct Reading {
  SignalId signal;
  std::int64_t raw;
  std::int8_t decimal_scale;
  Unit unit;
  Quality quality;
};

struct RegisterBlockView {
  RegisterTable table;
  std::uint16_t start;
  const std::uint16_t* values;
  std::size_t count;
};

inline double scaled_value(const Reading& reading) {
  double value = static_cast<double>(reading.raw);
  for (std::int8_t scale = reading.decimal_scale; scale < 0; ++scale) {
    value /= 10.0;
  }
  for (std::int8_t scale = reading.decimal_scale; scale > 0; --scale) {
    value *= 10.0;
  }
  return value;
}

const char* error_name(Error error);
const char* signal_name(SignalId signal);
const char* unit_symbol(Unit unit);

}  // namespace haipower
