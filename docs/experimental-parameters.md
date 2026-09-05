# Experimental register catalogue

This is an **opt-in interpretation catalogue, not 297 hardware-qualified
measurements**. Include `<sandisolar/experimental.h>` and keep the
`sandisolar::experimental::` prefix visible. There are 284 input parameters and
13 read-only holding parameters; the existing 30 profile fields are included,
not added again. The existing profile and SignalId values remain unchanged.

## Evidence, not a confidence percentage

Each descriptor AND successful result carries independent evidence for
**meaning**, **encoding** and **scale/unit**:

| Evidence | What it says |
|---|---|
| Unknown | We do not know the physical interpretation; retain raw words. |
| ProtocolMap | An interpretation is specified by the inspected map, not verified on every model. |
| Hypothesis | An explicit proposal, with the limitation in the descriptor note. |
| ConflictingSources | Evidence disagrees; the supplied conversion remains only a proposal. |

There is deliberately no unqualified `Verified` or `100%` label. A valid CRC,
a zero register or an existing `Quality::Good` flag is not proof that a model
implements a physical sensor. Applicability is also retained: model-dependent,
not single-phase according to the map, or single-phase according to the map.
These are source restrictions, not runtime device identification.

Display/use uncertain values if useful, but keep their evidence and the
catalogue revision alongside any exported sample. A changed interpretation
must not silently turn historical raw evidence into a confirmed measurement.
This catalogue does not gate values on speculative engineering limits.

## Reading and reinterpreting

```cpp
#include <sandisolar.h>
#include <sandisolar/experimental.h>
namespace ex = sandisolar::experimental;

// block must originate from a SUCCESSFUL validated transport result.
const auto* parameter = ex::find_parameter(sandisolar::RegisterTable::Input, 10);
if (parameter) {
  const auto result = ex::decode_parameter(*parameter, block);
  if (result) {
    double suggestedTemperature = ex::proposed_value(result.value);
    auto meaning = result.value.meaning_evidence; // ProtocolMap, not confirmed
    auto scale = result.value.scale_evidence;
    auto originalWord = result.value.raw.words[0];
    // Suggested name: parameter->proposed_name. Stable key: input_010.
  }
}

// Your research may identify another use: interpret the ORIGINAL words.
const auto raw = sandisolar::decode_register_value(
    block, 10, sandisolar::Encoding::U16);
if (raw) {
  double myOwnMeaning = raw.value.value / 100.0; // explicitly your hypothesis
}
```

Keys identify a table and register address (`input_010`, `holding_058`), not a
semantic claim. The proposed label may be corrected without renaming the
address. Raw decoding also works for addresses absent from this catalogue,
including reserved/opaque registers, **only if already present in a validated
block**. It never manufactures a sensor name, enables a feature, or initiates
I/O. Copy/retain original words, table, address and the encoding choice.

`ParameterReading` owns one/two original U16 words and the assembled signed or
unsigned value. The proposed physical conversion is
`raw.value * scale_numerator / scale_denominator`. The rational scale handles
documented half-minute operating-time ticks as 30 seconds without modifying raw
evidence. No borrowed source pointer is retained in a result. A short/wrong-table
block is an error, not a zero sample. Invalid encoding, invalid register-space
range and zero denominator are rejected.

For BLE, reuse `BleReadSession::read` (FC03/FC04, at most 125 registers per
request) and its existing CRC, correlation and lifecycle checks. To inspect
the input region previously read on the field unit, explicitly schedule starts
0, 125, 250 and 375, each count125, one at a time. Consume each result and
observe the session command gap; do not claim four pages are one atomic sample.
Keep each page's arrival time and error. No scheduler or automatic
holding-register sweep is introduced by this catalogue.

The [DecodeExperimentalPage example](../examples/DecodeExperimentalPage/DecodeExperimentalPage.ino)
shows catalogue iteration and custom interpretation with synthetic data.
Its `inspectBlock` function can also consume successful pages from the existing
BLE example. It performs no extra reads.

## Coverage and intentional exclusions

The catalogue covers system status/fault words, inverter/ambient/NTC/IPM/LLC
temperatures, bus/grid quantities, PV1..16, generator and EPS quantities, BMS
status/faults/warnings, capacities, cell extrema, cycles, power flows, energy
counters and parallel-system aggregates. Alarm words remain numeric: no new
fault-description dictionary or bit meanings were invented.

ASCII identity strings, reserved rows and undocumented byte placement are not
silently promoted to new typed sensors. Input455 is deliberately exposed as the
whole U16 word although the map says uint8; byte placement remains a hypothesis.
Multiword continuation rows are part of their preceding parameter, not duplicate
readings. Holding63/64 are a proposed combined nominal-power value. Only map
entries explicitly marked R were selected from the holding table; write-only
65/66 and R/W configuration (including Bluetooth231) are absent from this
catalogue. The pre-existing low-level FC03 builder remains available; this
catalogue is not a new allowlist for arbitrary register probing.

SOC128 retains the existing whole-percent proposal with conflicting-source
evidence. Other SOC fields with an ambiguous 0.01 unit remain raw. SOH152's
percent scale is a hypothesis. Generator apparent-power fields keep the source
W unit and flag its conflict with apparent power rather than silently changing
to VA. Opaque DSP/BMS debug words have Unknown meaning. Duplicated names for
parallel load energy at474/476 are marked Hypothesis.

## Compatibility and provenance

The existing profile decoder and this catalogue share the new meaning-free
`decode_register_value` primitive. The profile keeps its existing names,
scales, Quality flags and return type; no uncertain field is quietly added to
the normal SignalId API. Unknown encodings and impossible register-block ranges
now fail explicitly instead of falling through to an arbitrary 32-bit decode.
Only Unit values were appended; old IDs retain their numbers.

Inspected source: the vendor-formatted Modbus V2.14-1 workbook distributed by
[bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration](https://github.com/bootuz-dinamon/Sandisolar-Aohai-AO-6KSL-G3-Home-Assistant-integration).
Only factual mappings and independently chosen labels are implemented here; the
workbook, APK, decompiled source and private captures are not redistributed.
The register catalogue is static, has no heap or I/O, and is linked only when
used. It is not an automatic model resolver or a general plugin framework.
The reference metadata revision is `aohai-v214-experimental-r1`.

## Full catalogue

All rows are experimental and model-dependent unless the applicability column
states a narrower map restriction. Evidence columns are **meaning / encoding /
scale**; all three remain visible even where they differ.

| Key | Proposed name | Encoding | Proposed conversion | Evidence (M/E/S) | Applicability |
|---|---|---|---|---|---|
| input_000 | inverter_status | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_001 | start_delay_time | U16 | raw × 1/1 s | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_002 | inverter_voltage_r | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_003 | inverter_voltage_s | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_004 | inverter_voltage_t | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_005 | inverter_current_r | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_006 | inverter_current_s | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_007 | inverter_current_t | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_008 | bus1_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_009 | bus2_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_010 | inverter_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_011 | boost_ipm_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_012 | llc_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_013 | battery_ntc_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_014 | ambient_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_015 | dc_voltage_component_r | S16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_016 | dc_voltage_component_s | S16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_017 | dc_voltage_component_t | S16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_018 | dc_current_component_r | S16 | raw × 1/1000 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_019 | dc_current_component_s | S16 | raw × 1/1000 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_020 | dc_current_component_t | S16 | raw × 1/1000 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_021 | insulation_resistance | U16 | raw × 1000/1 ohm | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_022 | leakage_current | U16 | raw × 1/1000 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_023 | history_event_count | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_024 | system_fault_word_0 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_025 | system_fault_word_1 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_026 | system_fault_word_2 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_027 | system_fault_word_3 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_028 | system_fault_word_4 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_029 | system_fault_word_5 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_030 | system_fault_word_6 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_031 | system_fault_word_7 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_032 | inverter_fault_code | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_033 | inverter_warning_code | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_034 | inverter_fault_subcode | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_035 | inverter_warning_subcode | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_036 | device_type | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_038 | derating_mode | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_039 | power_lead_lag_flag | U16 | raw × 1/1 raw / dimensionless | Hypothesis / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_040 | positive_bus_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_041 | negative_bus_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_042 | grid_voltage_r | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_043 | grid_current_r | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_044 | grid_voltage_s | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_045 | grid_current_s | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_046 | grid_voltage_t | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_047 | grid_current_t | S16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_048 | grid_line_voltage_rs | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_049 | grid_line_voltage_st | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_050 | grid_line_voltage_tr | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_051 | grid_frequency | U16 | raw × 1/100 Hz | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_052 | power_factor | S16 | raw × 1/10000 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_053 | actual_output_percent | U16 | raw × 1/1 raw / dimensionless | Hypothesis / ProtocolMap / Unknown | ModelDependent |
| input_054 | eps_frequency | U16 | raw × 1/100 Hz | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_055 | eps_voltage_r | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_056 | eps_current_r | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_057 | eps_voltage_s | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_058 | eps_current_s | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_059 | eps_voltage_t | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_060 | eps_current_t | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_063 | pv_channel_count | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_064 | pv1_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_065 | pv1_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_066 | pv2_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_067 | pv2_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_068 | pv3_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_069 | pv3_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_070 | pv4_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_071 | pv4_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_072 | pv5_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_073 | pv5_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_074 | pv6_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_075 | pv6_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_076 | pv7_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_077 | pv7_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_078 | pv8_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_079 | pv8_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_080 | pv9_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_081 | pv9_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_082 | pv10_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_083 | pv10_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_084 | pv11_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_085 | pv11_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_086 | pv12_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_087 | pv12_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_088 | pv13_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_089 | pv13_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_090 | pv14_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_091 | pv14_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_092 | pv15_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_093 | pv15_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_094 | pv16_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_095 | pv16_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_096 | generator_voltage_r | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_097 | generator_current_r | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_098 | generator_voltage_s | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_099 | generator_current_s | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_100 | generator_voltage_t | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_101 | generator_current_t | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_102 | generator_frequency | U16 | raw × 1/100 Hz | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_106 | fan_speed | U16 | raw × 1/1 rpm | ProtocolMap / ProtocolMap / ProtocolMap | SinglePhaseAccordingToMap |
| input_107 | operating_time | U32BigEndian | raw × 30/1 s | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_109 | dsp_debug_word_1 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_110 | dsp_debug_word_2 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_111 | dsp_debug_word_3 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_112 | dsp_debug_word_4 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_113 | dsp_debug_word_5 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_114 | dsp_debug_word_6 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_115 | dsp_debug_word_7 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_116 | dsp_debug_word_8 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_117 | dsp_debug_word_9 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_118 | dsp_debug_word_10 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_119 | dsp_debug_word_11 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_120 | dsp_debug_word_12 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_121 | dsp_debug_word_13 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_122 | dsp_debug_word_14 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_123 | dsp_debug_word_15 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_124 | dsp_debug_word_16 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | ModelDependent |
| input_125 | battery_priority | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_126 | battery_type | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_127 | battery_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_128 | battery_soc | U16 | raw × 1/1 % | ProtocolMap / ProtocolMap / ConflictingSources | ModelDependent |
| input_129 | battery_voltage_dsp | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_130 | bms_status | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_131 | bms_error_word_1 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_132 | bms_error_word_2 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_133 | bms_error_word_3 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_134 | bms_error_word_4 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_135 | bms_warning_word_1 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_136 | bms_warning_word_2 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_137 | bms_warning_word_3 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_138 | bms_warning_word_4 | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_141 | battery_current | S16 | raw × 1/100 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_142 | battery_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_143 | battery_max_charge_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_144 | battery_max_discharge_current | U16 | raw × 1/10 A | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_145 | battery_rated_capacity | U16 | raw × 1/10 Ah | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_146 | battery_remaining_capacity | U16 | raw × 1/10 Ah | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_147 | bms_software_major | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_148 | bms_software_minor | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_149 | bms_hardware_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_150 | battery_cell_voltage_delta | U16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_151 | battery_cycle_count | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_152 | battery_soh | U16 | raw × 1/1 % | ProtocolMap / ProtocolMap / Hypothesis | ModelDependent |
| input_153 | battery_recommended_charge_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_154 | battery_lv_voltage | U16 | raw × 1/10 V | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_155 | bms_info_word | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_156 | battery_pack_info_word | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_157 | battery_max_cell_voltage | U16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_158 | battery_min_cell_voltage | U16 | raw × 1/1000 V | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_159 | battery_module_count | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_160 | battery_cell_count | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_161 | battery_max_voltage_cell_index | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_162 | battery_min_voltage_cell_index | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_163 | battery_max_cell_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_164 | battery_min_cell_temperature | S16 | raw × 1/10 C | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_165 | battery_max_temperature_cell_index | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_166 | battery_min_temperature_cell_index | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_167 | battery_protection_pack_id | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_168 | battery_max_soc | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / Unknown | ModelDependent |
| input_169 | battery_min_soc | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / Unknown | ModelDependent |
| input_170 | bms_company_code | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_171 | battery_energy_pack_id | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_172 | battery_pack_discharge_energy | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_174 | battery_pack_charge_energy | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_186 | bms_debug_word_1 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_187 | bms_debug_word_2 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_188 | bms_debug_word_3 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_189 | bms_debug_word_4 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_190 | bms_debug_word_5 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_191 | bms_debug_word_6 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_192 | bms_debug_word_7 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_193 | bms_debug_word_8 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_194 | bms_debug_word_9 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_195 | bms_debug_word_10 | U16 | raw × 1/1 raw / dimensionless | Unknown / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_250 | pv_total_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_252 | pv1_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_254 | pv2_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_256 | pv3_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_258 | pv4_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_260 | pv5_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_262 | pv6_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_264 | pv7_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_266 | pv8_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_268 | pv9_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_270 | pv10_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_272 | pv11_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_274 | pv12_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_276 | pv13_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_278 | pv14_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_280 | pv15_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_282 | pv16_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_284 | output_apparent_power_total | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_286 | output_active_power_total | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_288 | output_reactive_power_total | S32BigEndian | raw × 1/10 var | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_290 | output_apparent_power_r | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_292 | output_active_power_r | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_294 | output_reactive_power_r | S32BigEndian | raw × 1/10 var | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_296 | output_apparent_power_s | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_298 | output_active_power_s | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_300 | output_reactive_power_s | S32BigEndian | raw × 1/10 var | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_302 | output_apparent_power_t | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_304 | output_active_power_t | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_306 | output_reactive_power_t | S32BigEndian | raw × 1/10 var | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_314 | power_to_user_r | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_316 | power_to_user_s | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_318 | power_to_user_t | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_320 | power_to_user_total | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_322 | grid_export_power_r | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_324 | grid_export_power_s | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_326 | grid_export_power_t | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_328 | grid_export_power_total | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_330 | load_power_total | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_332 | eps_apparent_power_r | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_334 | eps_active_power_r | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_336 | eps_apparent_power_s | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_338 | eps_active_power_s | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_340 | eps_apparent_power_t | U32BigEndian | raw × 1/10 VA | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_342 | eps_active_power_t | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_344 | eps_load_percent | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / Unknown | NotSinglePhaseAccordingToMap |
| input_345 | system_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_347 | self_consumption_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_349 | battery_discharge_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_351 | battery_charge_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_353 | ac_charge_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_355 | extra_inverter_grid_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_357 | generator_apparent_power_r | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ConflictingSources | ModelDependent |
| input_359 | generator_apparent_power_s | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ConflictingSources | ModelDependent |
| input_361 | generator_apparent_power_t | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ConflictingSources | ModelDependent |
| input_363 | generator_active_power_r | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_365 | generator_active_power_s | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_367 | generator_active_power_t | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_375 | output_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_377 | output_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_379 | pv_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_381 | pv_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_383 | battery_charge_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_385 | battery_charge_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_387 | battery_discharge_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_389 | battery_discharge_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_391 | ac_charge_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_393 | ac_charge_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_395 | extra_inverter_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_397 | extra_inverter_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| input_399 | system_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_401 | system_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_403 | self_consumption_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_405 | self_consumption_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_407 | load_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_409 | load_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_411 | grid_export_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_413 | grid_export_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_415 | grid_import_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_417 | grid_import_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_419 | sold_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_421 | sold_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_423 | purchased_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_425 | purchased_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_427 | self_to_load_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_429 | self_to_load_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_450 | parallel_role | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_455 | parallel_device_id | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / Hypothesis / Unknown | ModelDependent |
| input_456 | parallel_pv_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_458 | parallel_grid_power | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_460 | parallel_load_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_462 | parallel_battery_power | S32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_464 | parallel_self_consumption_power | U32BigEndian | raw × 1/10 W | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_466 | parallel_pv_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_468 | parallel_pv_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_470 | parallel_self_consumption_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_472 | parallel_self_consumption_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_474 | parallel_load_energy_today | U32BigEndian | raw × 1/10 kWh | Hypothesis / ProtocolMap / ProtocolMap | ModelDependent |
| input_476 | parallel_load_energy_total | U32BigEndian | raw × 1/10 kWh | Hypothesis / ProtocolMap / ProtocolMap | ModelDependent |
| input_478 | parallel_sold_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_480 | parallel_sold_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_482 | parallel_purchased_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_484 | parallel_purchased_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_486 | parallel_self_to_load_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_488 | parallel_self_to_load_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_490 | parallel_battery_soc | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / Unknown | ModelDependent |
| input_491 | parallel_battery_charge_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_493 | parallel_battery_charge_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_495 | parallel_battery_discharge_energy_today | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| input_497 | parallel_battery_discharge_energy_total | U32BigEndian | raw × 1/10 kWh | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_002 | device_type | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_033 | arm_firmware_number | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_036 | dsp1_firmware_number | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_037 | dsp2_firmware_number | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_038 | dsp1_test_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_039 | dsp2_test_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_040 | arm_test_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_048 | dsp_hardware_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_049 | arm_hardware_version | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |
| holding_058 | protocol_version_raw | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / Unknown | ModelDependent |
| holding_063 | nominal_inverter_power | U32BigEndian | raw × 1/10 W | ProtocolMap / Hypothesis / ProtocolMap | ModelDependent |
| holding_067 | firmware_update_progress_raw | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | ModelDependent |
| holding_093 | anti_backflow_host_no_response | U16 | raw × 1/1 raw / dimensionless | ProtocolMap / ProtocolMap / ProtocolMap | NotSinglePhaseAccordingToMap |

### Per-parameter qualifications

- `input_013`: External lead-acid battery NTC input according to map; not the BMS temperature at input_142.
- `input_039`: Map marks the meaning as pending.
- `input_053`: Map lists 0.01 without a clear reference/unit; conversion deliberately left raw. Map marks the meaning as pending.
- `input_109`: Opaque debug word; label does not identify a physical quantity.
- `input_110`: Opaque debug word; label does not identify a physical quantity.
- `input_111`: Opaque debug word; label does not identify a physical quantity.
- `input_112`: Opaque debug word; label does not identify a physical quantity.
- `input_113`: Opaque debug word; label does not identify a physical quantity.
- `input_114`: Opaque debug word; label does not identify a physical quantity.
- `input_115`: Opaque debug word; label does not identify a physical quantity.
- `input_116`: Opaque debug word; label does not identify a physical quantity.
- `input_117`: Opaque debug word; label does not identify a physical quantity.
- `input_118`: Opaque debug word; label does not identify a physical quantity.
- `input_119`: Opaque debug word; label does not identify a physical quantity.
- `input_120`: Opaque debug word; label does not identify a physical quantity.
- `input_121`: Opaque debug word; label does not identify a physical quantity.
- `input_122`: Opaque debug word; label does not identify a physical quantity.
- `input_123`: Opaque debug word; label does not identify a physical quantity.
- `input_124`: Opaque debug word; label does not identify a physical quantity.
- `input_128`: Whole-percent proposal matches one unit at raw 100; map says 0.01. Not universally qualified.
- `input_152`: SOH whole-percent proposal; map omits the unit/scale. Single-unit comparison is not universal qualification.
- `input_168`: Map lists 0.01 without a clear reference/unit; conversion deliberately left raw.
- `input_169`: Map lists 0.01 without a clear reference/unit; conversion deliberately left raw.
- `input_186`: Opaque debug word; label does not identify a physical quantity.
- `input_187`: Opaque debug word; label does not identify a physical quantity.
- `input_188`: Opaque debug word; label does not identify a physical quantity.
- `input_189`: Opaque debug word; label does not identify a physical quantity.
- `input_190`: Opaque debug word; label does not identify a physical quantity.
- `input_191`: Opaque debug word; label does not identify a physical quantity.
- `input_192`: Opaque debug word; label does not identify a physical quantity.
- `input_193`: Opaque debug word; label does not identify a physical quantity.
- `input_194`: Opaque debug word; label does not identify a physical quantity.
- `input_195`: Opaque debug word; label does not identify a physical quantity.
- `input_344`: Map lists 0.01 without a clear reference/unit; conversion deliberately left raw.
- `input_357`: Name says apparent power, unit says W rather than VA; retain proposed W scaling with explicit conflict.
- `input_359`: Name says apparent power, unit says W rather than VA; retain proposed W scaling with explicit conflict.
- `input_361`: Name says apparent power, unit says W rather than VA; retain proposed W scaling with explicit conflict.
- `input_455`: Map says uint8 without byte placement; preserve the entire 16-bit register, never silently mask.
- `input_458`: Map sign convention: export negative, import positive; not field-qualified.
- `input_462`: Map sign convention: charge negative, discharge positive; not field-qualified.
- `input_474`: Proposed today/total comes from descriptions; duplicated variable names in source are inconsistent.
- `input_476`: Proposed today/total comes from descriptions; duplicated variable names in source are inconsistent.
- `input_490`: Map lists 0.01 without a clear reference/unit; conversion deliberately left raw.
- `holding_033`: Numeric firmware/version component only; no guessed string formatting.
- `holding_036`: Numeric firmware/version component only; no guessed string formatting.
- `holding_037`: Numeric firmware/version component only; no guessed string formatting.
- `holding_038`: Numeric firmware/version component only; no guessed string formatting.
- `holding_039`: Numeric firmware/version component only; no guessed string formatting.
- `holding_040`: Numeric firmware/version component only; no guessed string formatting.
- `holding_058`: Raw protocol-version number. No decimal formatting or equivalence to the workbook version is inferred.
- `holding_063`: Combine high/low uint16 at 63/64; single-unit field evidence supports this layout, not every model.
- `holding_067`: Read-only progress/error code from map; does not start or configure an upgrade.
