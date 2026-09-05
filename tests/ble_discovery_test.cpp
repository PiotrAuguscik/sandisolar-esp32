#include "sandisolar.h"
#include <cstring>
#include <iostream>
#include <vector>

int main() {
  using namespace sandisolar;
  int failures=0;
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';++failures;} } while(false)
  const std::uint8_t name[]={11,9,'W','F','1','2','3','4','5','6','7','8'};
  auto a=parse_ble_advertisement(name,sizeof(name),nullptr,0);
  CHECK(a && a.value.wf_name && !a.value.service_00ff && a.value.name_size==10);
  std::uint8_t service[]={3,3,0xff,0};
  a=parse_ble_advertisement(service,sizeof(service),name,sizeof(name));
  CHECK(a && a.value.service_00ff);
  const std::uint8_t uuid128[]={17,7,0xfb,0x34,0x9b,0x5f,0x80,0,0,0x80,0,0x10,0,0,0xff,0,0,0};
  a=parse_ble_advertisement(uuid128,sizeof(uuid128),name,sizeof(name));
  CHECK(a && a.value.service_00ff);
  std::vector<std::uint8_t> broken(name,name+sizeof(name));broken[1]=8;
  CHECK(parse_ble_advertisement(broken.data(),broken.size(),nullptr,0).error==Error::NotFound);
  CHECK(parse_ble_advertisement(broken.data(),broken.size(),name,sizeof(name)));
  broken[1]=9;broken[3]=0;
  CHECK(!parse_ble_advertisement(broken.data(),broken.size(),nullptr,0));
  broken[3]=0xff;CHECK(!parse_ble_advertisement(broken.data(),broken.size(),nullptr,0));
  CHECK(!parse_ble_advertisement(name,sizeof(name)-1,nullptr,0));
  CHECK(!parse_ble_advertisement(nullptr,1,nullptr,0));
  CHECK(!parse_ble_advertisement(name,32,nullptr,0));
  broken.assign(name,name+sizeof(name));broken[2]='X';
  CHECK(!parse_ble_advertisement(broken.data(),broken.size(),name,sizeof(name))); // conflicting complete names
  broken.assign(name,name+sizeof(name));broken.push_back(0);broken.push_back(0);
  CHECK(parse_ble_advertisement(broken.data(),broken.size(),nullptr,0));
  broken.back()=1;CHECK(!parse_ble_advertisement(broken.data(),broken.size(),nullptr,0));
  const std::uint8_t odd[]={2,3,0xff};
  CHECK(!parse_ble_advertisement(odd,sizeof(odd),name,sizeof(name)));
  const std::uint8_t other[]={4,9,'H','S','1'};
  a=parse_ble_advertisement(other,sizeof(other),nullptr,0);
  CHECK(a && !a.value.wf_name && !a.value.service_00ff);
  BleScanResult scan{};BleScanCandidate c{};
  c.address[0]=2;c.advertisement=parse_ble_advertisement(name,sizeof(name),nullptr,0).value;
  merge_ble_candidate(scan,c);merge_ble_candidate(scan,c);CHECK(scan.count==1);
  CHECK(select_ble_candidate(scan));
  CHECK(select_ble_candidate(scan,{"Other",5}).error==Error::NotFound);
  CHECK(select_ble_candidate(scan,{nullptr,3}).error==Error::InvalidArgument);
  c.advertisement.name[31]='x';merge_ble_candidate(scan,c);
  CHECK(!scan.incomplete && scan.count==1); // unused buffer tail is not identity
  c.advertisement.name_size=255;merge_ble_candidate(scan,c);CHECK(scan.incomplete);
  scan={};c={};merge_ble_candidate(scan,c);CHECK(scan.incomplete && scan.count==0);
  scan.count=17;CHECK(select_ble_candidate(scan).error==Error::InvalidArgument);
  scan={};scan.error=Error::Timeout;CHECK(select_ble_candidate(scan).error==Error::Timeout);
  scan={};scan.incomplete=true;CHECK(select_ble_candidate(scan).error==Error::IncompleteScan);
  std::cout<<"Discovery failures: "<<failures<<'\n';return failures?1:0;
}
