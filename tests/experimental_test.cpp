#include "haipower.h"
#include "haipower/experimental.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

namespace {
using namespace haipower;
namespace ex = haipower::experimental;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while(false)

void raw_values_are_owned_and_reinterpretable() {
  std::uint16_t words[]={0xffe7,0xffff};
  RegisterBlockView block={RegisterTable::Input,10,words,2};
  const auto signed_value=decode_register_value(block,10,Encoding::S16);
  const auto unsigned_value=decode_register_value(block,10,Encoding::U16);
  CHECK(signed_value && signed_value.value.value==-25 && signed_value.value.words[0]==0xffe7);
  CHECK(unsigned_value && unsigned_value.value.value==65511);
  const auto* p=ex::find_parameter(RegisterTable::Input,10);
  CHECK(p && p->meaning_evidence==ex::Evidence::ProtocolMap);
  const auto reading=ex::decode_parameter(*p,block);
  CHECK(reading && ex::proposed_value(reading.value)==-2.5);
  CHECK(reading.value.raw.encoding==Encoding::S16 && reading.value.unit==Unit::Celsius);
  words[0]=0;
  CHECK(reading.value.raw.words[0]==0xffe7 && reading.value.raw.value==-25);
  // Unknown numeric addresses are still accessible without a made-up name.
  CHECK(ex::find_parameter(RegisterTable::Input,60000)==nullptr);
  const auto unknown=decode_register_value({RegisterTable::Input,60000,words,2},60000,Encoding::U32BigEndian);
  CHECK(unknown && unknown.value.value==65535);
  CHECK(decode_register_value(block,10,static_cast<Encoding>(255)).error==Error::InvalidArgument);
  CHECK(decode_register_value(block,11,Encoding::U32BigEndian).error==Error::FieldOutOfRange);
  CHECK(decode_register_value({RegisterTable::Input,65535,words,2},65535,Encoding::U16).error==Error::InvalidArgument);
  CHECK(decode_register_value({RegisterTable::Input,65535,words,1},65535,Encoding::U16));
  CHECK(decode_register_value({static_cast<RegisterTable>(2),0,words,2},0,Encoding::U16).error==Error::InvalidArgument);
  CHECK(decode_register_value({RegisterTable::Input,0,nullptr,2},0,Encoding::U16).error==Error::FieldOutOfRange);
  words[0]=0x8000;words[1]=0;
  CHECK(decode_register_value(block,10,Encoding::S16).value.value==-32768);
  CHECK(decode_register_value(block,10,Encoding::S32BigEndian).value.value==-2147483648LL);
  words[0]=words[1]=0xffff;
  CHECK(decode_register_value(block,10,Encoding::S32BigEndian).value.value==-1);
  CHECK(decode_register_value(block,10,Encoding::U32BigEndian).value.value==4294967295LL);
  FieldSpec invalid_field{SignalId::BatterySoc,RegisterTable::Input,10,
      static_cast<Encoding>(255),0,Unit::Percent,Quality::Good};
  const ProfileView invalid_profile={"invalid-encoding-test",&invalid_field,1};
  CHECK(decode_signal(invalid_profile,SignalId::BatterySoc,block).error==Error::InvalidArgument);
  CHECK(!find_field({"empty",nullptr,1},SignalId::BatterySoc));
}

void every_catalogue_entry_retains_evidence_and_rejects_partial_blocks() {
  const auto& catalog=ex::aohai_v214_catalog();
  CHECK(catalog.count==297);
  CHECK(std::strcmp(catalog.revision,"aohai-v214-experimental-r1")==0);
  std::set<std::string> keys;
  std::set<unsigned> addresses;
  unsigned input_count=0,holding_count=0;
  for(std::size_t i=0;i<catalog.count;++i) {
    const auto& p=catalog.parameters[i];
    CHECK(p.key && p.proposed_name && p.note && p.scale_denominator>0 && p.scale_numerator>0);
    CHECK(keys.insert(p.key).second);
    CHECK(addresses.insert((static_cast<unsigned>(p.table)<<16)|p.address).second);
    const bool input=p.table==RegisterTable::Input;
    if(input)++input_count;else ++holding_count;
    CHECK(ex::find_parameter(p.table,p.address)==&p);
    CHECK(std::strcmp(unit_symbol(p.unit),"?")!=0);
    const std::uint16_t words[]={0x1234,0xabcd};
    const unsigned n=(p.encoding==Encoding::U16 || p.encoding==Encoding::S16)?1:2;
    const auto r=ex::decode_parameter(p,{p.table,p.address,words,n});
    CHECK(r && r.value.raw.words[0]==words[0] && r.value.raw.word_count==n);
    CHECK(r.value.meaning_evidence==p.meaning_evidence && r.value.scale_evidence==p.scale_evidence);
    CHECK(r.value.encoding_evidence==p.encoding_evidence && r.value.applicability==p.applicability);
    CHECK(r.value.raw.address==p.address && r.value.raw.table==p.table);
    CHECK(std::abs(ex::proposed_value(r.value)-double(r.value.raw.value)*p.scale_numerator/p.scale_denominator)<0.00001);
    CHECK(ex::decode_parameter(p,{p.table,p.address,words,n-1}).error==Error::FieldOutOfRange);
    CHECK(ex::decode_parameter(p,{input?RegisterTable::Holding:RegisterTable::Input,p.address,words,n}).error==Error::FieldOutOfRange);
    auto invalid=p;invalid.scale_denominator=0;
    CHECK(ex::decode_parameter(invalid,{p.table,p.address,words,n}).error==Error::InvalidArgument);
  }
  CHECK(input_count==284 && holding_count==13);
  // Write-only factory-reset and upgrade command words are NOT catalog entries.
  CHECK(!ex::find_parameter(RegisterTable::Holding,65));
  CHECK(!ex::find_parameter(RegisterTable::Holding,66));
  // Configuration registers are not added just because a map contains them.
  CHECK(!ex::find_parameter(RegisterTable::Holding,231));
  CHECK(!ex::find_parameter(RegisterTable::Input,37)); // reserved, no fake interpretation
}

void uncertainty_is_not_silently_promoted() {
  const auto* soc=ex::find_parameter(RegisterTable::Input,128);
  CHECK(soc->meaning_evidence==ex::Evidence::ProtocolMap);
  CHECK(soc->scale_evidence==ex::Evidence::ConflictingSources);
  CHECK(soc->unit==Unit::Percent && soc->scale_numerator==1 && soc->scale_denominator==1);
  const std::uint16_t raw[]={100};
  auto r=ex::decode_parameter(*soc,{RegisterTable::Input,128,raw,1});
  CHECK(r && ex::proposed_value(r.value)==100 && r.value.scale_evidence==ex::Evidence::ConflictingSources);
  CHECK(ex::find_parameter(RegisterTable::Input,168)->scale_evidence==ex::Evidence::Unknown);
  CHECK(ex::find_parameter(RegisterTable::Input,152)->scale_evidence==ex::Evidence::Hypothesis);
  CHECK(ex::find_parameter(RegisterTable::Input,109)->meaning_evidence==ex::Evidence::Unknown);
  CHECK(ex::find_parameter(RegisterTable::Input,357)->scale_evidence==ex::Evidence::ConflictingSources);
  CHECK(ex::find_parameter(RegisterTable::Input,357)->unit==Unit::Watt); // conflict retained, not silently changed to VA
  CHECK(ex::find_parameter(RegisterTable::Input,455)->encoding_evidence==ex::Evidence::Hypothesis);
  const std::uint16_t packed[]={0xab05};
  r=ex::decode_parameter(*ex::find_parameter(RegisterTable::Input,455),{RegisterTable::Input,455,packed,1});
  CHECK(r && r.value.raw.value==0xab05); // no unverified uint8 masking
  CHECK(ex::find_parameter(RegisterTable::Input,476)->meaning_evidence==ex::Evidence::Hypothesis);
  CHECK(ex::find_parameter(RegisterTable::Input,3)->applicability==ex::Applicability::NotSinglePhaseAccordingToMap);
  CHECK(ex::find_parameter(RegisterTable::Input,106)->applicability==ex::Applicability::SinglePhaseAccordingToMap);
  CHECK(ex::find_parameter(RegisterTable::Holding,58)->scale_evidence==ex::Evidence::Unknown);
  const std::uint16_t zero[]={0};
  r=ex::decode_parameter(*ex::find_parameter(RegisterTable::Input,109),{RegisterTable::Input,109,zero,1});
  CHECK(r && r.value.raw.value==0 && r.value.meaning_evidence==ex::Evidence::Unknown);
}

void independent_map_examples_and_legacy_profile_remain_consistent() {
  struct Case { unsigned address; unsigned count; std::uint16_t hi,lo; double value; Unit unit; };
  const Case cases[]={
    {107,2,0,120,3600,Unit::Second}, // documented half-minutes -> seconds
    {21,1,5,0,5000,Unit::Ohm},
    {150,1,18,0,0.018,Unit::Volt},
    {157,1,3320,0,3.32,Unit::Volt},
    {163,1,0xffec,0,-2,Unit::Celsius},
    {288,2,0xffff,0xff9c,-10,Unit::Var},
    {332,2,0,1234,123.4,Unit::VoltAmpere},
    {458,2,0xffff,0xd8f0,-1000,Unit::Watt},
    {497,2,1,0,6553.6,Unit::KilowattHour}
  };
  for(const auto& c:cases) {
    std::uint16_t words[]={c.hi,c.lo};
    const auto* p=ex::find_parameter(RegisterTable::Input,c.address);
    const auto r=ex::decode_parameter(*p,{RegisterTable::Input,static_cast<std::uint16_t>(c.address),words,c.count});
    CHECK(r && r.value.unit==c.unit && std::abs(ex::proposed_value(r.value)-c.value)<0.000001);
  }
  for(unsigned i=0;i<16;++i) {
    const auto* v=ex::find_parameter(RegisterTable::Input,64+2*i);
    const auto* a=ex::find_parameter(RegisterTable::Input,65+2*i);
    const auto* w=ex::find_parameter(RegisterTable::Input,252+2*i);
    CHECK(v->unit==Unit::Volt && a->unit==Unit::Ampere && w->unit==Unit::Watt);
    CHECK(v->scale_denominator==10 && a->scale_denominator==10 && w->scale_denominator==10);
    CHECK(w->encoding==Encoding::U32BigEndian);
  }
  const auto& profile=aohai_modbus_v214_profile();
  CHECK(profile.field_count==30 && static_cast<unsigned>(SignalId::PvEnergyTotal)==29);
  CHECK(static_cast<unsigned>(Unit::AmpereHour)==8 && static_cast<unsigned>(Quality::UnverifiedScale)==1);
  std::uint16_t data[]={0x0032,0x0101};
  for(std::size_t i=0;i<profile.field_count;++i) {
    const auto& f=profile.fields[i];
    const auto* p=ex::find_parameter(f.table,f.address);
    const RegisterBlockView block={f.table,f.address,data,2};
    const auto old=decode_signal(profile,f.signal,block);
    const auto now=ex::decode_parameter(*p,block);
    CHECK(old && now && old.value.raw==now.value.raw.value);
    CHECK(old.value.unit==now.value.unit && std::abs(scaled_value(old.value)-ex::proposed_value(now.value))<0.00001);
  }
}
}

int main(int argc,char** argv) {
  // Machine-readable export for independent source-map checks / user tooling.
  if(argc==2 && std::string(argv[1])=="--list") {
    const auto& c=ex::aohai_v214_catalog();
    for(std::size_t i=0;i<c.count;++i) {
      const auto& p=c.parameters[i];
      std::cout << p.key << '\t' << p.proposed_name << '\t' << unsigned(p.encoding)
        << '\t' << p.scale_numerator << '\t' << p.scale_denominator << '\t'
        << unit_symbol(p.unit) << '\t' << ex::evidence_name(p.meaning_evidence) << '\t'
        << ex::evidence_name(p.encoding_evidence) << '\t' << ex::evidence_name(p.scale_evidence)
        << '\t' << ex::applicability_name(p.applicability) << '\n';
    }
    return 0;
  }
  if(argc!=1) return 2;
  raw_values_are_owned_and_reinterpretable();
  every_catalogue_entry_retains_evidence_and_rejects_partial_blocks();
  uncertainty_is_not_silently_promoted();
  independent_map_examples_and_legacy_profile_remain_consistent();
  std::cout << "Experimental catalogue failures: " << failures << '\n';
  return failures ? 1 : 0;
}
