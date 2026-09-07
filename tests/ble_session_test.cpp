#include "haipower.h"
#include <cstring>
#include <deque>
#include <iostream>
#include <vector>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while (false)
// Identity transform isolates session semantics; real AES and golden frames
// are exercised separately by ble_openssl_tests, not simulated here.
bool copy_crypto(const std::uint8_t*, const std::uint8_t*, const std::uint8_t* in,
                 std::size_t n, std::uint8_t* out, void*) {
  std::memcpy(out, in, n);
  return true;
}
const haipower::Aes128Cbc crypto = {copy_crypto, copy_crypto, nullptr};
const haipower::BleTarget target = {{0x02,0,0,0,0,1}, haipower::BleAddressType::Public,
                                   {"WF12345678", 10}};
const haipower::BleReadRequest soc = {1, haipower::RegisterTable::Input, 128, 1};
using haipower::Error;
using haipower::BleSessionState;

struct Link : haipower::BleLink {
  using Kind = EventType;
  std::deque<Event> events;
  std::vector<std::uint8_t> last_write;
  unsigned opens = 0, closes = 0;
  std::uint32_t epoch = 0;
  Error write_error = Error::None;
  Error open_error = Error::None;
  Error open(const haipower::BleTarget& t, std::uint32_t e) override {
    CHECK(t.logger.size == 10 && std::memcmp(t.logger.data, "WF12345678", 10) == 0);
    ++opens; epoch = e; return open_error;
  }
  Error write(const std::uint8_t* p, std::size_t n) override {
    last_write.assign(p, p+n); return write_error;
  }
  void close() override { ++closes; }
  bool poll(Event& out) override {
    if (events.empty()) return false;
    out = events.front(); events.pop_front(); return true;
  }
  void push(Kind kind, Error error = Error::None, std::uint16_t mtu = 512) {
    Event e{}; e.type=kind; e.epoch=epoch; e.error=error; e.mtu=mtu;
    events.push_back(e);
  }
  void response(unsigned value, std::uint8_t correlation, std::uint32_t at,
                bool corrupt = false) {
    Event e{}; e.type=Kind::Notification; e.epoch=epoch; e.at_ms=at; e.size=42;
    e.bytes[1]=40; e.bytes[3]=1; e.bytes[5]=34;
    e.bytes[6]=correlation; e.bytes[7]=0x17;
    std::memcpy(e.bytes+8, "WF12345678", 10);
    e.bytes[19]=7; e.bytes[20]=1; e.bytes[21]=4; e.bytes[22]=2;
    e.bytes[23]=static_cast<std::uint8_t>(value>>8);
    e.bytes[24]=static_cast<std::uint8_t>(value);
    auto crc=haipower::crc16_modbus(e.bytes+20,5);
    e.bytes[25]=crc&255; e.bytes[26]=crc>>8;
    crc=haipower::crc16_modbus(e.bytes,40);
    e.bytes[40]=crc>>8; e.bytes[41]=(crc&255) ^ (corrupt ? 1 : 0);
    events.push_back(e);
  }
  void response_words(const std::vector<std::uint16_t>& words,
                      std::uint8_t correlation, std::uint32_t at) {
    const std::size_t rtu_size = 5 + 2 * words.size();
    const std::size_t padded = ((12 + rtu_size + 15) / 16) * 16;
    Event e{}; e.type=Kind::Notification; e.epoch=epoch; e.at_ms=at; e.size=8+padded+2;
    CHECK(!words.empty() && words.size()<=125 && e.size<=sizeof(e.bytes));
    if (words.empty() || words.size()>125 || e.size>sizeof(e.bytes)) return;
    e.bytes[0]=(e.size-2)>>8; e.bytes[1]=(e.size-2)&255; e.bytes[3]=1;
    e.bytes[4]=(padded+2)>>8; e.bytes[5]=(padded+2)&255;
    e.bytes[6]=correlation; e.bytes[7]=0x17;
    std::memcpy(e.bytes+8,"WF12345678",10);
    e.bytes[18]=rtu_size>>8; e.bytes[19]=rtu_size&255;
    e.bytes[20]=1; e.bytes[21]=4; e.bytes[22]=2*words.size();
    for (std::size_t i=0; i<words.size(); ++i) {
      e.bytes[23+2*i]=words[i]>>8; e.bytes[24+2*i]=words[i]&255;
    }
    auto crc=haipower::crc16_modbus(e.bytes+20,rtu_size-2);
    e.bytes[20+rtu_size-2]=crc&255; e.bytes[20+rtu_size-1]=crc>>8;
    crc=haipower::crc16_modbus(e.bytes,e.size-2);
    e.bytes[e.size-2]=crc>>8; e.bytes[e.size-1]=crc&255;
    events.push_back(e);
  }
};

void ready(Link& link, haipower::BleReadSession& session, std::uint32_t now=0,
           std::uint16_t mtu=512) {
  CHECK(session.connect(target,now)==Error::None);
  link.push(Link::Kind::Ready,Error::None,mtu); session.poll(now);
  CHECK(session.state()==BleSessionState::Ready);
}

void success_requires_valid_packet_and_att_completion() {
  Link link; haipower::BleReadSession s(link,crypto); ready(link,s);
  CHECK(s.read(soc,10)==Error::None);
  CHECK(s.read(soc,11)==Error::Busy);
  CHECK(link.last_write.size()==42 && link.last_write[6]==0);
  link.response(97,0,15); s.poll(16);
  haipower::BleReadResult result{};
  CHECK(!s.take(result));
  link.push(Link::Kind::Written); s.poll(17);
  CHECK(s.read(soc,2000)==Error::Busy); // unread owned result
  CHECK(s.take(result) && result.error==Error::None && result.registers[0]==97);
  CHECK(result.received_at_ms==15 && result.requested_at_ms==10 && result.epoch==1);
  CHECK(!s.take(result));
  CHECK(s.read(soc,1016)==Error::Busy);
  CHECK(s.read(soc,1017)==Error::None);
  CHECK(link.last_write[6]==1);
  // old/duplicated correlation and old epoch cannot complete a new operation.
  link.response(10,0,1020); link.push(Link::Kind::Written);
  link.response(20,1,1021); link.events.back().epoch=0;
  s.poll(1022); CHECK(!s.take(result));
  CHECK(s.last_packet_error()==Error::WrongCorrelation);
  link.response(98,1,1023,true); s.poll(1023);
  CHECK(s.last_packet_error()==Error::BadCrc && !s.take(result));
  link.response(98,1,1024); s.poll(1024);
  CHECK(s.take(result) && result.registers[0]==98);
  s.disconnect(1025); CHECK(s.state()==BleSessionState::Closing);
  CHECK(s.connect(target,1026)==Error::Busy);
  link.push(Link::Kind::Closed); s.poll(1027);
  CHECK(s.state()==BleSessionState::Idle);
  ready(link,s,1028); CHECK(link.epoch==2);
  CHECK(s.read(soc,1028)==Error::Busy); // reconnect cannot bypass command gap
  CHECK(s.read(soc,2028)==Error::None);
  link.response(1,0,2029); link.events.back().epoch=1;
  link.push(Link::Kind::Written); s.poll(2029); CHECK(!s.take(result));
  link.response(99,0,2030); s.poll(2030);
  CHECK(s.take(result) && result.epoch==2 && result.registers[0]==99);
}

void timeout_closes_and_never_publishes_a_late_or_cached_value() {
  Link link; haipower::BleReadSession s(link,crypto); ready(link,s);
  CHECK(s.read(soc,1)==Error::None);
  link.push(Link::Kind::Written); link.response(100,0,10001);
  s.poll(10001);
  haipower::BleReadResult result{};
  CHECK(s.take(result) && result.error==Error::Timeout && result.received_at_ms==0);
  CHECK(result.registers[0]==0 && s.state()==BleSessionState::Closing && link.closes==1);
  s.poll(20001); CHECK(s.state()==BleSessionState::Fault);
  link.push(Link::Kind::Closed); s.poll(20002);
  CHECK(s.state()==BleSessionState::Fault); // no unsafe automatic recovery
}

void capacity_validation_and_wraparound() {
  Link link; haipower::BleReadSession s(link,crypto);
  auto invalid=target; invalid.logger={nullptr,10};
  CHECK(s.connect(invalid,0)==Error::InvalidArgument && link.opens==0);
  ready(link,s,0,44);
  CHECK(s.read(soc,0)==Error::MtuTooSmall && link.last_write.empty());
  auto bad=soc; bad.count=0;
  CHECK(s.read(bad,0)==Error::InvalidArgument);
  s.disconnect(0); link.push(Link::Kind::Closed); s.poll(0);
  // Unsigned millis subtraction across the 49-day wrap.
  ready(link,s,0xfffffff0);
  CHECK(s.read(soc,0xfffffff0)==Error::None);
  link.response(88,0,0xfffffff2); link.push(Link::Kind::Written); s.poll(0xfffffff3);
  haipower::BleReadResult r{}; CHECK(s.take(r));
  CHECK(s.read(soc,0x10)==Error::Busy);
  CHECK(s.read(soc,0x400)==Error::None);
}

void correlation_is_never_reused_in_a_connection() {
  Link link; haipower::BleReadSession s(link,crypto); ready(link,s);
  haipower::BleReadResult r{};
  for (unsigned i=0;i<255;++i) {
    CHECK(s.read(soc,i*2000)==Error::None);
    CHECK(link.last_write[6]==i);
    link.push(Link::Kind::Written); link.response(80,i,i*2000+1);
    s.poll(i*2000+1); CHECK(s.take(r) && r.error==Error::None);
  }
  CHECK(s.read(soc,510000)==Error::CorrelationExhausted);
  CHECK(link.opens==1 && link.closes==0); // caller decides reconnect policy
}

void sustained_reads_cross_four_fenced_session_renewals() {
  Link link; haipower::BleReadSession s(link,crypto); ready(link,s);
  haipower::BleReadResult result{};
  for (unsigned i=0;i<1254;++i) {
    const std::uint32_t now=i*30000;
    if (i && i%255==0) {
      CHECK(s.read(soc,now)==Error::CorrelationExhausted);
      s.disconnect(now);
      CHECK(s.connect(target,now)==Error::Busy);
      link.push(Link::Kind::Closed); s.poll(now+1);
      CHECK(s.state()==BleSessionState::Idle);
      ready(link,s,now+2);
    }
    CHECK(s.read(soc,now+3)==Error::None);
    CHECK(link.last_write[6]==i%255);
    link.push(Link::Kind::Written);
    if (i && i%255==0) {
      link.response(65535,0,now+4); link.events.back().epoch=link.epoch-1;
      s.poll(now+4); CHECK(!s.take(result)); // old epoch cannot satisfy new read
    }
    link.response(i%101,i%255,now+5); s.poll(now+5);
    CHECK(s.take(result) && result.error==Error::None);
    CHECK(result.epoch==i/255+1 && result.registers[0]==i%101);
  }
  CHECK(link.opens==5 && link.closes==4);
  s.disconnect(1254*30000);
  link.push(Link::Kind::Closed); s.poll(1254*30000+1);
  CHECK(s.state()==BleSessionState::Idle && link.closes==5);
}

void disconnect_cancel_and_transport_failures_are_terminal_once() {
  for (int scenario=0;scenario<5;++scenario) {
    Link link; haipower::BleReadSession s(link,crypto); ready(link,s);
    if (scenario==0) link.write_error=Error::TransportFailure;
    CHECK(s.read(soc,5)==Error::None);
    if (scenario==1) link.push(Link::Kind::Written,Error::TransportFailure);
    if (scenario==2) link.push(Link::Kind::Closed,Error::NotConnected);
    if (scenario==3) s.disconnect(6);
    if (scenario==4) link.push(Link::Kind::Fault,Error::TransportFault);
    s.poll(7);
    haipower::BleReadResult r{};
    CHECK(s.take(r) && r.error!=Error::None && r.registers[0]==0);
    CHECK(!s.take(r));
  }
  Link link; haipower::BleReadSession s(link,crypto);
  CHECK(s.connect(target,0)==Error::None); s.poll(30000);
  CHECK(s.state()==BleSessionState::Closing && s.last_error()==Error::Timeout);
  haipower::BleReadResult r{}; CHECK(!s.take(r));
}

void named_reads_use_existing_transport_and_error_boundaries() {
  using haipower::SignalId;
  const auto& profile = haipower::aohai_modbus_v214_profile();
  Link link; haipower::BleReadSession s(link,crypto);
  CHECK(haipower::read_signal(s,profile,SignalId::BatterySoc,0)==Error::NotConnected);
  CHECK(link.opens==0 && link.last_write.empty()); // helper never auto-connects
  ready(link,s);
  const SignalId bad[] = {SignalId::BatterySoc, static_cast<SignalId>(65535)};
  CHECK(haipower::read_signals(s,profile,bad,0)==Error::UnsupportedSignal);
  const SignalId distant[] = {SignalId::BatterySoc,SignalId::PvEnergyTotal};
  CHECK(haipower::read_signals(s,profile,distant,0)==Error::FieldOutOfRange);
  const SignalId single[] = {SignalId::BatterySoc};
  CHECK(haipower::read_signals(s,profile,single,10,0)==Error::InvalidArgument);
  // Explicit pointer/count call must not be confused with array/time/slave.
  CHECK(haipower::read_signals(s,profile,single,1,10,0)==Error::InvalidArgument);
  CHECK(haipower::read_signal(s,profile,SignalId::BatterySoc,0,0)==Error::InvalidArgument);
  CHECK(link.last_write.empty() && s.state()==BleSessionState::Ready);
  CHECK(haipower::read_signal(s,profile,SignalId::BatterySoc,10)==Error::None);
  // Identity crypto exposes the original RTU request; compare the full packet
  // against the existing low-level codec, not just an assigned struct.
  std::uint8_t expected[512]{};
  const auto built = haipower::build_ble_page_read(target.logger,1,
      haipower::RegisterTable::Input,128,1,0,crypto,expected,sizeof(expected));
  CHECK(built && built.value==link.last_write.size());
  CHECK(std::memcmp(expected,link.last_write.data(),built.value)==0);
  CHECK(haipower::read_signal(s,profile,SignalId::BatterySoc,11)==Error::Busy);
  link.response(97,0,12); link.push(Link::Kind::Written); s.poll(13);
  haipower::BleReadResult result{};
  CHECK(s.take(result));
  const auto decoded = haipower::decode_signal(profile,SignalId::BatterySoc,result);
  CHECK(decoded && decoded.value.raw==97 && decoded.value.quality==haipower::Quality::UnverifiedScale);
  CHECK(haipower::read_signal(s,profile,SignalId::BatterySoc,14)==Error::Busy);
  const SignalId group[] = {SignalId::BatterySoc,SignalId::BatterySoh,SignalId::BatteryVoltage};
  CHECK(haipower::read_signals(s,profile,group,1100)==Error::None);
  const auto grouped = haipower::build_ble_page_read(target.logger,1,
      haipower::RegisterTable::Input,127,26,1,crypto,expected,sizeof(expected));
  CHECK(grouped && grouped.value==link.last_write.size());
  CHECK(std::memcmp(expected,link.last_write.data(),grouped.value)==0);
  std::vector<std::uint16_t> words(26,0); // synthetic group, NOT a real capture
  words[0]=530; words[1]=77; words[14]=0xffc4; words[15]=212; words[25]=98;
  link.push(Link::Kind::Written); link.response_words(words,1,1102); s.poll(1103);
  CHECK(s.take(result) && result.error==Error::None && result.request.start==127 && result.request.count==26);
  CHECK(result.received_at_ms==1102 && result.requested_at_ms==1100);
  CHECK(haipower::decode_signal(profile,SignalId::BatterySoc,result).value.raw==77);
  CHECK(haipower::decode_signal(profile,SignalId::BatterySoh,result).value.raw==98);
  CHECK(haipower::scaled_value(haipower::decode_signal(profile,SignalId::BatteryVoltage,result).value)==53);
  const auto current = haipower::decode_signal(profile,SignalId::BatteryCurrent,result);
  CHECK(current && current.value.raw==-60 && current.value.decimal_scale==-2);
  CHECK(haipower::read_signals(s,profile,group,2200,2)==Error::None);
  CHECK(link.last_write[20]==2); // optional slave is not mistaken for count/time
  s.poll(12200); // failed grouped read must not decode as valid zero SOC
  CHECK(s.take(result) && result.error==Error::Timeout);
  CHECK(result.requested_at_ms==2200 && result.request.slave==2 && result.request.count==26);
  CHECK(haipower::decode_signal(profile,SignalId::BatterySoc,result).error==Error::Timeout);
  CHECK(s.state()==BleSessionState::Closing && link.closes==1);
  link.push(Link::Kind::Closed); s.poll(12201);
  CHECK(s.state()==BleSessionState::Idle);
}
}

int main() {
  success_requires_valid_packet_and_att_completion();
  timeout_closes_and_never_publishes_a_late_or_cached_value();
  capacity_validation_and_wraparound();
  correlation_is_never_reused_in_a_connection();
  sustained_reads_cross_four_fenced_session_renewals();
  disconnect_cancel_and_transport_failures_are_terminal_once();
  named_reads_use_existing_transport_and_error_boundaries();
  std::cout << "BLE session failures: " << failures << '\n';
  return failures ? 1 : 0;
}
