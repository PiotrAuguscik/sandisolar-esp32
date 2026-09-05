#pragma once
#include "haipower/register_value.h"

// Opt-in interpretations of the broad register map, NOT qualified device APIs.
// Keep this namespace in call sites: no semantic promise is implied by a name.
namespace haipower {
namespace experimental {

// Evidence of an interpretation, not probability, CRC validity or freshness.
// In particular ProtocolMap != hardware verified on the caller's model.
enum class Evidence : std::uint8_t {
  Unknown = 0, ProtocolMap, Hypothesis, ConflictingSources
};
enum class Applicability : std::uint8_t {
  ModelDependent = 0, NotSinglePhaseAccordingToMap, SinglePhaseAccordingToMap
};
struct ParameterSpec {
  const char* key;             // stable table/address key, e.g. input_010
  const char* proposed_name;   // convenience label; may be corrected later
  RegisterTable table;
  std::uint16_t address;
  Encoding encoding;
  std::int32_t scale_numerator;
  std::uint32_t scale_denominator;
  Unit unit;
  Evidence meaning_evidence;
  Evidence encoding_evidence;
  Evidence scale_evidence;
  Applicability applicability;
  const char* note;            // contradictions/unknown details; never NULL
};
struct CatalogView {
  const char* revision;
  const ParameterSpec* parameters;
  std::size_t count;
};
struct ParameterReading {
  RawRegisterValue raw;
  std::int32_t scale_numerator;
  std::uint32_t scale_denominator;
  Unit unit;
  Evidence meaning_evidence;
  Evidence encoding_evidence;
  Evidence scale_evidence;
  Applicability applicability;
};

// Static process-lifetime catalogue; it never scans, reads or enables a device.
const CatalogView& aohai_v214_catalog();
const ParameterSpec* find_parameter(RegisterTable table, std::uint16_t address);
Result<ParameterReading> decode_parameter(const ParameterSpec& parameter,
                                          const RegisterBlockView& block);
// Only use on a successful decode. RawRegisterValue remains unchanged.
double proposed_value(const ParameterReading& reading);
const char* evidence_name(Evidence evidence);
const char* applicability_name(Applicability applicability);

}  // namespace experimental
}  // namespace haipower
