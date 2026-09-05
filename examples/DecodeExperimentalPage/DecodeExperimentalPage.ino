#include <sandisolar.h>
#include <sandisolar/experimental.h>

namespace ex = sandisolar::experimental;

// Use this function on each SUCCESSFUL BleReadResult. It does not perform any
// additional reads. Keep result.received_at_ms separately for freshness.
void inspectBlock(const sandisolar::RegisterBlockView& block) {
  const auto& catalog=ex::aohai_v214_catalog();
  for(size_t i=0;i<catalog.count;++i) {
    const auto& p=catalog.parameters[i];
    const auto decoded=ex::decode_parameter(p,block);
    if(!decoded) continue; // field missing from this page, NOT a zero value
    const auto& r=decoded.value;
    Serial.printf("%s proposed=%s value=%.3f%s raw=%lld words=%04x/%04x "
                  "meaning=%s encoding=%s scale=%s applicability=%s\n",
        p.key,p.proposed_name,ex::proposed_value(r),sandisolar::unit_symbol(r.unit),
        r.raw.value,r.raw.words[0],r.raw.words[1],
        ex::evidence_name(r.meaning_evidence),ex::evidence_name(r.encoding_evidence),
        ex::evidence_name(r.scale_evidence),ex::applicability_name(r.applicability));
    if(p.note[0]) Serial.println(p.note);
  }
}

void setup() {
  Serial.begin(115200);
  // Synthetic words, NOT a real reading or proof of an inverter temperature.
  const uint16_t words[]={0xffe7};
  const sandisolar::RegisterBlockView block={sandisolar::RegisterTable::Input,10,words,1};
  inspectBlock(block); // proposed temperature -2.5 C; meaning from map only

  // If your research shows another use, reinterpret the ORIGINAL word:
  const auto raw=sandisolar::decode_register_value(block,10,sandisolar::Encoding::U16);
  if(raw) {
    Serial.printf("Own interpretation: unsigned=%lld, custom scale /100=%.2f\n",
                  raw.value.value,raw.value.value/100.0);
  }
}

void loop() {}
