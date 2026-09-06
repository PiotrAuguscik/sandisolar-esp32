#include "haipower.h"
#include "haipower/arduino_bluedroid_link.h"
#include "bluedroid_shim/sdk.h"
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while (false)
std::uint32_t now=0;
int controller=0;
int wifi_mode_error=ESP_ERR_WIFI_NOT_INIT, wifi_ps_error=0;
wifi_mode_t wifi_mode=WIFI_MODE_STA;
wifi_ps_type_t wifi_ps=WIFI_PS_MIN_MODEM;
int wifi_queries=0, power_queries=0;
int controller_init_error=0, controller_enable_error=0;
bool controller_init_stuck=false, controller_enable_stuck=false;
Callback callback=nullptr;
GapCallback gap_callback=nullptr;
struct TestQueue {
  std::deque<std::vector<std::uint8_t>> values;
  std::size_t event_size=0;
  unsigned capacity=0;
};
TestQueue queues[3];
unsigned queue_count=0;
int scan_submit_error=0;
int scan_start_error=0;
std::uint32_t expected_seconds=1;
std::vector<std::string> calls;
std::vector<std::uint8_t> transmitted;
unsigned characteristic_count=1;
const haipower::BleTarget target={{0x02,0,0,0,0,1},haipower::BleAddressType::Public,
                                 {"WF12345678",10}};
bool copy_crypto(const std::uint8_t*,const std::uint8_t*,const std::uint8_t* p,
                 std::size_t n,std::uint8_t* out,void*) { std::memcpy(out,p,n); return true; }
const haipower::Aes128Cbc crypto={copy_crypto,copy_crypto,nullptr};
const haipower::BleReadRequest soc={1,haipower::RegisterTable::Input,128,1};
using haipower::Error;
using haipower::BleSessionState;
int call(const char* name) { calls.emplace_back(name); return 0; }
void pump(haipower::BleReadSession& s) { for(unsigned i=0;i<16;++i) s.poll(now); }
void event(esp_gattc_cb_event_t type,esp_ble_gattc_cb_param_t p={},std::uint8_t iface=3) {
  // ESP-IDF 4.4 btc_gattc_cb_handler passes NULL for UNREG, unlike REG.
  callback(type,iface,type==ESP_GATTC_UNREG_EVT ? nullptr : &p);
}
void up_to_discovery(haipower::BleReadSession& s) {
  CHECK(s.connect(target,now)==Error::None);
  esp_ble_gattc_cb_param_t p{}; p.reg.app_id=0x4841;
  event(ESP_GATTC_REG_EVT,p); pump(s); CHECK(calls.back()=="open");
  p={}; p.open.conn_id=7; std::memcpy(p.open.remote_bda,target.address,6);
  event(ESP_GATTC_OPEN_EVT,p); pump(s); CHECK(calls.back()=="mtu");
  p={}; p.cfg_mtu.conn_id=7; p.cfg_mtu.mtu=512;
  event(ESP_GATTC_CFG_MTU_EVT,p); pump(s); CHECK(calls.back()=="search");
  p={}; p.search_res.conn_id=7; p.search_res.start_handle=40; p.search_res.end_handle=48;
  p.search_res.srvc_id.uuid={2,{0xff}};
  event(ESP_GATTC_SEARCH_RES_EVT,p); pump(s);
  p={}; p.search_cmpl.conn_id=7; event(ESP_GATTC_SEARCH_CMPL_EVT,p); pump(s);
}
void ready(haipower::BleReadSession& s) {
  up_to_discovery(s); CHECK(calls.back()=="notify");
  esp_ble_gattc_cb_param_t p{}; p.reg_for_notify.handle=42;
  event(ESP_GATTC_REG_FOR_NOTIFY_EVT,p); pump(s); CHECK(calls.back()=="cccd");
  CHECK(s.state()==BleSessionState::Connecting); // registration != subscription
  p={}; p.write.conn_id=7; p.write.handle=43;
  event(ESP_GATTC_WRITE_DESCR_EVT,p); pump(s);
  CHECK(s.state()==BleSessionState::Ready);
}
void closed(haipower::BleReadSession& s) {
  esp_ble_gattc_cb_param_t p{}; p.close.conn_id=7;
  event(ESP_GATTC_CLOSE_EVT,p); pump(s);
  CHECK(s.state()!=BleSessionState::Idle && calls.back()=="unregister");
  CHECK(s.connect(target,now)==Error::Busy);
  event(ESP_GATTC_UNREG_EVT); pump(s);
  CHECK(s.state()==BleSessionState::Idle);
}
void response(haipower::BleReadSession& s,std::uint8_t corr,unsigned value,
              std::uint16_t handle=42) {
  std::uint8_t wire[42]={}; wire[1]=40;wire[3]=1;wire[5]=34;wire[6]=corr;wire[7]=0x17;
  std::memcpy(wire+8,"WF12345678",10); wire[19]=7;wire[20]=1;wire[21]=4;wire[22]=2;
  wire[23]=value>>8;wire[24]=value&255;
  auto crc=haipower::crc16_modbus(wire+20,5);wire[25]=crc&255;wire[26]=crc>>8;
  crc=haipower::crc16_modbus(wire,40);wire[40]=crc>>8;wire[41]=crc&255;
  esp_ble_gattc_cb_param_t p{};p.notify.conn_id=7;p.notify.handle=handle;
  p.notify.value_len=42;p.notify.value=wire;std::memcpy(p.notify.remote_bda,target.address,6);
  event(ESP_GATTC_NOTIFY_EVT,p);
  std::memset(wire,0,sizeof(wire)); // vendor callback storage no longer valid
  pump(s);
}
void written(haipower::BleReadSession& s) {
  esp_ble_gattc_cb_param_t p{};p.write.conn_id=7;p.write.handle=42;
  event(ESP_GATTC_WRITE_CHAR_EVT,p);pump(s);
}
}

// Minimal dependency double. Assertions check effective UUIDs, target, ATT
// response policy and CCCD bytes at the actual production API call sites.
QueueHandle_t xQueueCreate(unsigned n,std::size_t size) {
  CHECK(queue_count<3);if(queue_count>=3)return nullptr;
  auto& q=queues[queue_count++];q.event_size=size;q.capacity=n;return &q;
}
int xQueueSend(QueueHandle_t handle,const void* data,int wait) {
  auto& q=*static_cast<TestQueue*>(handle);
  CHECK(wait==0); if(q.values.size()==q.capacity)return 0;
  const auto* p=static_cast<const std::uint8_t*>(data);q.values.emplace_back(p,p+q.event_size);return 1;
}
int xQueueReceive(QueueHandle_t handle,void* data,int wait) {
  auto& q=*static_cast<TestQueue*>(handle);
  CHECK(wait==0);if(q.values.empty())return 0;
  std::memcpy(data,q.values.front().data(),q.event_size);q.values.pop_front();return 1;
}
std::uint32_t millis(){return now;}
int esp_wifi_get_mode(wifi_mode_t* mode){++wifi_queries;*mode=wifi_mode;return wifi_mode_error;}
int esp_wifi_get_ps(wifi_ps_type_t* ps){++power_queries;*ps=wifi_ps;return wifi_ps_error;}
int esp_bt_controller_init(esp_bt_controller_config_t* config){
  CHECK(config->configured==42);call("controller_init");
  if (!controller_init_error && !controller_init_stuck) controller=ESP_BT_CONTROLLER_STATUS_INITED;
  return controller_init_error;
}
int esp_bt_controller_enable(esp_bt_mode_t mode){
#if defined(CONFIG_BTDM_CONTROLLER_MODE_BTDM)
  CHECK(mode==ESP_BT_MODE_BTDM);
#elif defined(CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY)
  CHECK(mode==ESP_BT_MODE_CLASSIC_BT);
#else
  CHECK(mode==ESP_BT_MODE_BLE);
#endif
  call("controller_enable");
  if (!controller_enable_error && !controller_enable_stuck) controller=ESP_BT_CONTROLLER_STATUS_ENABLED;
  return controller_enable_error;
}
int esp_bt_controller_get_status(){return controller;}
int esp_bluedroid_get_status(){return 0;}
int esp_bluedroid_init(){return call("bt_init");}
int esp_bluedroid_enable(){return call("bt_enable");}
int esp_ble_gattc_register_callback(Callback cb){callback=cb;return call("callback");}
int esp_ble_gap_register_callback(GapCallback cb){gap_callback=cb;return call("gap_callback");}
int esp_ble_gap_set_scan_params(esp_ble_scan_params_t* p){
  CHECK(p->scan_type==BLE_SCAN_TYPE_ACTIVE && p->own_addr_type==BLE_ADDR_TYPE_PUBLIC);
  CHECK(p->scan_filter_policy==BLE_SCAN_FILTER_ALLOW_ALL && p->scan_duplicate==BLE_SCAN_DUPLICATE_DISABLE);
  CHECK(p->scan_interval==160 && p->scan_window==80);call("scan_params");return scan_submit_error;
}
int esp_ble_gap_start_scanning(std::uint32_t seconds){CHECK(seconds==expected_seconds);call("scan_start");return scan_start_error;}
int esp_ble_gap_stop_scanning(){return call("scan_stop");}
int esp_ble_gatt_set_local_mtu(std::uint16_t mtu){CHECK(mtu==512);return call("local_mtu");}
int esp_ble_gattc_app_register(std::uint16_t id){CHECK(id==0x4841);return call("register");}
int esp_ble_gattc_app_unregister(esp_gatt_if_t iface){CHECK(iface==3);return call("unregister");}
int esp_ble_gattc_open(esp_gatt_if_t iface,std::uint8_t* addr,esp_ble_addr_type_t type,bool direct){
  CHECK(iface==3 && type==BLE_ADDR_TYPE_PUBLIC && direct && !std::memcmp(addr,target.address,6));return call("open");
}
int esp_ble_gattc_close(esp_gatt_if_t iface,std::uint16_t conn){CHECK(iface==3 && conn==7);return call("close");}
int esp_ble_gap_disconnect(std::uint8_t* addr){CHECK(!std::memcmp(addr,target.address,6));return call("disconnect");}
int esp_ble_gattc_send_mtu_req(esp_gatt_if_t iface,std::uint16_t conn){CHECK(iface==3 && conn==7);return call("mtu");}
int esp_ble_gattc_search_service(esp_gatt_if_t,std::uint16_t,esp_bt_uuid_t* uuid){CHECK(uuid->uuid.uuid16==0xff);return call("search");}
int esp_ble_gattc_get_char_by_uuid(esp_gatt_if_t,std::uint16_t,std::uint16_t start,
    std::uint16_t end,esp_bt_uuid_t uuid,esp_gattc_char_elem_t* out,std::uint16_t* count){
  CHECK(start==40 && end==48 && uuid.uuid.uuid16==0xff01 && *count==2);
  *count=characteristic_count;out[0]={42,24};return call("get_char");
}
int esp_ble_gattc_get_descr_by_char_handle(esp_gatt_if_t,std::uint16_t,std::uint16_t handle,
    esp_bt_uuid_t uuid,esp_gattc_descr_elem_t* out,std::uint16_t* count){
  CHECK(handle==42 && uuid.uuid.uuid16==0x2902 && *count==2);
  *count=1;out[0]={43,{2,{0x2902}}};return call("get_cccd");
}
int esp_ble_gattc_register_for_notify(esp_gatt_if_t,std::uint8_t*,std::uint16_t handle){CHECK(handle==42);return call("notify");}
int esp_ble_gattc_write_char_descr(esp_gatt_if_t,std::uint16_t,std::uint16_t handle,
    std::uint16_t size,std::uint8_t* bytes,int type,int auth){
  CHECK(handle==43 && size==2 && bytes[0]==1 && bytes[1]==0 && type==2 && auth==0);return call("cccd");
}
int esp_ble_gattc_write_char(esp_gatt_if_t,std::uint16_t,std::uint16_t handle,
    std::uint16_t size,std::uint8_t* bytes,int type,int auth){
  CHECK(handle==42 && size==42 && type==2 && auth==0);
  transmitted.assign(bytes,bytes+size);return call("write");
}

void gap(esp_gap_ble_cb_event_t type, esp_ble_gap_cb_param_t p={}) { gap_callback(type,&p); }
void scanning(haipower::BleScanner& scanner,haipower::BleLoggerIdentity name={nullptr,0}) {
  CHECK(scanner.start(expected_seconds,now,name)==Error::None);
  CHECK(scanner.start(1,now)==Error::Busy);
  gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT);scanner.poll(now);
  CHECK(calls.back()=="scan_start");
  gap(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT);scanner.poll(now);
  CHECK(scanner.state()==haipower::BleScanState::Scanning);
}
void advertised(unsigned address,const char* name,int rssi=-50,bool service=false,
                int address_type=BLE_ADDR_TYPE_PUBLIC) {
  esp_ble_gap_cb_param_t p{};auto& r=p.scan_rst;
  r.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;r.ble_addr_type=address_type;
  r.bda[0]=2;r.bda[5]=address;r.rssi=rssi;
  if(service){r.ble_adv[0]=3;r.ble_adv[1]=3;r.ble_adv[2]=0xff;r.adv_data_len=4;}
  auto* response=r.ble_adv+r.adv_data_len;
  const auto n=std::strlen(name);CHECK(n<=29);response[0]=n+1;response[1]=9;
  std::memcpy(response+2,name,n);r.scan_rsp_len=n+2;
  gap(ESP_GAP_BLE_SCAN_RESULT_EVT,p);
  std::memset(&p,0,sizeof(p)); // callback-owned storage is gone
}
void scan_finished(haipower::BleScanner& scanner){
  esp_ble_gap_cb_param_t p{};p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_CMPL_EVT;
  gap(ESP_GAP_BLE_SCAN_RESULT_EVT,p);scanner.poll(now);
  CHECK(scanner.state()==haipower::BleScanState::Complete);
}
void scan_tests(const std::string& mode) {
  using namespace haipower;
  auto& scanner=arduino_bluedroid_scanner();
  BleReadSession session(arduino_bluedroid_link(),crypto);
  BleScanResult result{};
  CHECK(scanner.start(0,now)==Error::InvalidArgument);
  CHECK(scanner.start(61,now)==Error::InvalidArgument);
  if(mode=="scan_config_timeout") {
    now=0xfffffff0;CHECK(scanner.start(1,now)==Error::None);
    now+=10000;scanner.poll(now);
  } else if(mode=="scan_control_overflow") {
    CHECK(scanner.start(1,now)==Error::None);
    for(unsigned i=0;i<5;++i)gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT);
    scanner.poll(now);
  } else if(mode=="scan_timeout") {
    scanning(scanner);now+=6000;scanner.poll(now);
  } else if(mode=="scan_duplicate_control") {
    scanning(scanner);gap(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT);scanner.poll(now);
  } else {
    scan_submit_error=1;CHECK(scanner.start(1,now)==Error::TransportFailure);
    CHECK(scanner.state()==BleScanState::Idle);scan_submit_error=0;
    CHECK(scanner.start(1,now)==Error::None);
    esp_ble_gap_cb_param_t p{};p.scan_param_cmpl.status=1;
    gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,p);scanner.poll(now);
    CHECK(scanner.take(result) && result.error==Error::TransportFailure);
    scan_start_error=1;CHECK(scanner.start(1,now)==Error::None);
    gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT);scanner.poll(now);
    CHECK(scanner.take(result) && result.error==Error::TransportFailure);scan_start_error=0;
    scanning(scanner);
    CHECK(session.connect(target,now)==Error::Busy); // no concurrent GATT open
    advertised(1,"WF12345678");advertised(1,"WF12345678",-42);
    advertised(2,"HS0123456789",-20); // unrelated battery is not a candidate
    advertised(3,"OtherLogger",-65,true,BLE_ADDR_TYPE_RANDOM);
    scan_finished(scanner);
    CHECK(session.connect(target,now)==Error::Busy); // result not consumed yet
    CHECK(scanner.take(result) && result.error==Error::None && !result.incomplete && result.count==2);
    CHECK(!scanner.take(result));
    CHECK(select_ble_candidate(result).error==Error::AmbiguousDevice);
    auto picked=select_ble_candidate(result,{"WF12345678",10});
    CHECK(picked && picked.value.rssi==-42 && picked.value.address[5]==1);
    const auto copied=picked.value; result={};
    CHECK(std::string(copied.target().logger.data,copied.target().logger.size)=="WF12345678");
    CHECK(copied.target().logger.data==copied.advertisement.name);
    // Exact-name filter is copied; input may be changed immediately.
    char filter[]="SpecialLogger";
    CHECK(scanner.start(1,now,{filter,13})==Error::None);filter[0]='X';
    gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT);scanner.poll(now);
    gap(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT);scanner.poll(now);
    advertised(1,"WF12345678");advertised(2,"SpecialLogger");
    scan_finished(scanner);CHECK(scanner.take(result) && result.count==1);
    CHECK(select_ble_candidate(result) && result.candidates[0].address[5]==2);
    // Same name on distinct addresses remains ambiguous, even when filtered.
    scanning(scanner);advertised(1,"WF12345678");advertised(2,"WF12345678");
    scan_finished(scanner);CHECK(scanner.take(result));
    CHECK(select_ble_candidate(result,{"WF12345678",10}).error==Error::AmbiguousDevice);
    // Candidate data overflow cannot drop the independent completion event.
    scanning(scanner);for(unsigned i=0;i<17;++i)advertised(i+1,"WF12345678");
    scan_finished(scanner);CHECK(scanner.take(result) && result.incomplete);
    CHECK(select_ble_candidate(result).error==Error::IncompleteScan);
    scanning(scanner);for(unsigned i=0;i<17;++i){advertised(i+1,"WF12345678");scanner.poll(now);}
    scan_finished(scanner);CHECK(scanner.take(result) && result.incomplete && result.count==16);
    // Conflict and public/random identity do not collapse into a guessed target.
    scanning(scanner);advertised(1,"WF12345678");advertised(1,"WF87654321");
    scan_finished(scanner);CHECK(scanner.take(result) && result.incomplete);
    scanning(scanner);advertised(1,"WF12345678");advertised(1,"WF12345678",-50,false,BLE_ADDR_TYPE_RANDOM);
    scan_finished(scanner);CHECK(scanner.take(result) && result.count==2);
    scanning(scanner);scan_finished(scanner);CHECK(scanner.take(result) && result.count==0);
    CHECK(select_ble_candidate(result).error==Error::NotFound); // no stale last scan
    // The same real production adapter can connect only after scan retirement.
    ready(session);CHECK(scanner.start(1,now)==Error::Busy);
    session.disconnect(now);closed(session);
    return;
  }
  CHECK(scanner.state()==BleScanState::Fault);
  CHECK(scanner.take(result) && result.error!=Error::None && result.incomplete);
  CHECK(!scanner.take(result));
  gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT);scanner.poll(now);
  CHECK(scanner.start(1,now)==Error::TransportFault);
  CHECK(session.connect(target,now)==Error::TransportFault);
}

void initialization_tests(const std::string& mode) {
  using haipower::initialize_arduino_bluedroid;
  CHECK(std::string(haipower::error_name(Error::RadioConflict))=="radio_conflict");
  const bool rejected=mode=="init_wifi_none_sta" || mode=="init_wifi_none_ap" ||
      mode=="init_wifi_none_apsta" || mode=="init_wifi_mode_error" ||
      mode=="init_wifi_ps_error" || mode=="init_wifi_ps_unknown";
  wifi_mode_error=0;
  if (mode=="init_wifi_absent") wifi_mode_error=ESP_ERR_WIFI_NOT_INIT;
  if (mode=="init_wifi_null") wifi_mode=WIFI_MODE_NULL;
  if (mode=="init_wifi_max") wifi_ps=WIFI_PS_MAX_MODEM;
  if (mode=="init_wifi_none_sta" || mode=="init_wifi_none_ap" || mode=="init_wifi_none_apsta")
    wifi_ps=WIFI_PS_NONE;
  if (mode=="init_wifi_none_ap" || mode=="init_wifi_ap") wifi_mode=WIFI_MODE_AP;
  if (mode=="init_wifi_none_apsta" || mode=="init_wifi_apsta") wifi_mode=WIFI_MODE_APSTA;
  if (mode=="init_wifi_mode_error") wifi_mode_error=-1;
  if (mode=="init_wifi_ps_error") wifi_ps_error=-1;
  if (mode=="init_wifi_ps_unknown") wifi_ps=static_cast<wifi_ps_type_t>(3);
  if (rejected) {
    const auto expected=wifi_ps==WIFI_PS_NONE ? Error::RadioConflict : Error::TransportFailure;
    CHECK(initialize_arduino_bluedroid()==expected);
    CHECK(calls.empty() && queue_count==0 && controller==0);
    // No attempted-state poison: fixing caller policy permits initialization.
    wifi_mode_error=wifi_ps_error=0;wifi_ps=WIFI_PS_MIN_MODEM;
  }
  if (mode=="init_controller_error") controller_init_error=-1;
  if (mode=="init_enable_error") controller_enable_error=-1;
  controller_init_stuck=mode=="init_controller_stuck";
  controller_enable_stuck=mode=="init_enable_stuck";
  const bool fault=controller_init_error || controller_enable_error ||
      controller_init_stuck || controller_enable_stuck;
  CHECK(initialize_arduino_bluedroid()==(fault ? Error::TransportFault : Error::None));
  if (fault) {
    CHECK(!callback && !gap_callback);
    CHECK(calls.size()==(controller_init_error || controller_init_stuck ? 1u : 2u));
  } else CHECK(callback && gap_callback && controller==ESP_BT_CONTROLLER_STATUS_ENABLED);
  const auto count=calls.size();
  CHECK(initialize_arduino_bluedroid()==(fault ? Error::TransportFault : Error::None));
  CHECK(calls.size()==count && queue_count==3);
  if (mode=="init_wifi_absent" || mode=="init_wifi_null") CHECK(power_queries==0);
}

int main(int argc,char** argv) {
  controller=1;
  CHECK(haipower::initialize_arduino_bluedroid()==Error::Busy && calls.empty());
  CHECK(wifi_queries==0 && queue_count==0);
  controller=0;
  if (argc>1 && std::string(argv[1]).find("init_")==0) {
    initialization_tests(argv[1]);
    std::cout<<"Initialization adapter failures: "<<failures<<'\n';return failures?1:0;
  }
  CHECK(haipower::initialize_arduino_bluedroid()==Error::None);
  if(argc>1){scan_tests(argv[1]);std::cout<<"Scan adapter failures: "<<failures<<'\n';return failures?1:0;}
  haipower::BleReadSession s(haipower::arduino_bluedroid_link(),crypto);
  ready(s);
  CHECK(haipower::arduino_bluedroid_scanner().start(1,now)==Error::Busy);
  CHECK(s.read(soc,now)==Error::None && transmitted[6]==0);
  response(s,0,97,47); // unrelated ff03/handle cannot publish a reading
  written(s);haipower::BleReadResult result{};CHECK(!s.take(result));
  response(s,0,97);CHECK(s.take(result) && result.error==Error::None && result.registers[0]==97);
  now+=1001;CHECK(s.read(soc,now)==Error::None && transmitted[6]==1);
  response(s,0,10);CHECK(!s.take(result)); // duplicate/late previous packet
  response(s,1,98);CHECK(!s.take(result)); // notification before ATT completion
  written(s);CHECK(s.take(result) && result.registers[0]==98);
  s.disconnect(now);closed(s);
  now+=1001;
  ready(s);CHECK(s.read(soc,now)==Error::None && transmitted[6]==0);
  written(s);response(s,0,99);CHECK(s.take(result) && result.epoch==2 && result.registers[0]==99);
  s.disconnect(now);closed(s);
  now+=1001;
  // Duplicate ff01 is not permission to select an arbitrary characteristic.
  characteristic_count=2;up_to_discovery(s);CHECK(calls.back()=="close");
  closed(s);CHECK(s.last_error()==Error::TransportFailure);characteristic_count=1;
  // A close requested before registration completion waits for the real barrier.
  CHECK(s.connect(target,now)==Error::None);s.disconnect(now);
  esp_ble_gattc_cb_param_t p{};p.reg.app_id=0x4841;
  event(ESP_GATTC_REG_EVT,p);pump(s);CHECK(calls.back()=="unregister");
  event(ESP_GATTC_UNREG_EVT);pump(s);CHECK(s.state()==BleSessionState::Idle);
  ready(s);CHECK(s.read(soc,now)==Error::None);
  // Queue overflow destroys lifecycle evidence and must fail closed.
  p={};p.write.conn_id=7;p.write.handle=42;
  for(unsigned i=0;i<9;++i)event(ESP_GATTC_WRITE_CHAR_EVT,p);
  pump(s);CHECK(s.state()==BleSessionState::Fault && s.last_error()==Error::TransportFault);
  CHECK(s.take(result) && result.error==Error::TransportFault);
  CHECK(s.connect(target,now)==Error::Busy);
  std::cout << "Bluedroid dependency-double failures: " << failures << '\n';
  return failures ? 1 : 0;
}
