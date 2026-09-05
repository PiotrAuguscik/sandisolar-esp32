// Independent AES adapter and fixed vectors made using OpenSSL CLI plus a
// standalone CRC routine (check value: "123456789" -> 0x4b37). Synthetic ID.
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "haipower.h"

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; ++failures; } } while (false)
bool transform(const std::uint8_t* key, const std::uint8_t* iv,
               const std::uint8_t* in, std::size_t size,
               std::uint8_t* out, void* encrypt) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return false;
  int length = 0, tail = 0;
  const bool ok = EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key, iv,
                                  encrypt != nullptr) == 1 &&
      EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
      EVP_CipherUpdate(ctx, out, &length, in, static_cast<int>(size)) == 1 &&
      EVP_CipherFinal_ex(ctx, out + length, &tail) == 1 &&
      static_cast<std::size_t>(length + tail) == size;
  EVP_CIPHER_CTX_free(ctx);
  return ok;
}
bool encrypt(const std::uint8_t* k, const std::uint8_t* iv,
             const std::uint8_t* in, std::size_t n, std::uint8_t* out, void*) {
  int mode = 1;
  return transform(k, iv, in, n, out, &mode);
}
std::vector<std::uint8_t> hex(const char* s) {
  std::vector<std::uint8_t> result;
  for (std::size_t i = 0; s[i]; i += 2)
    result.push_back(static_cast<std::uint8_t>(std::stoul(std::string(s+i, 2), nullptr, 16)));
  return result;
}
}

int main(int argc, char** argv) {
  const haipower::Aes128Cbc crypto = {encrypt, transform, nullptr};
  // Read-only replay of private field captures without putting identifiers or
  // captures in the public source. stdin: page_start correlation frame_hex.
  if (argc == 3 && std::string(argv[1]) == "--replay") {
    const haipower::BleLoggerIdentity logger = {argv[2],std::strlen(argv[2])};
    unsigned start = 0, correlation = 0, pages = 0;
    std::string text;
    while (std::cin >> start >> correlation >> text) {
      if (start > 65411 || correlation > 254 || text.size() > 1024 || text.size()%2) return 2;
      const auto frame = hex(text.c_str());
      std::uint16_t registers[125]{};
      const auto result = haipower::parse_ble_read_frame(frame.data(),frame.size(),
          logger,1,haipower::RegisterTable::Input,125,correlation,crypto,registers,125);
      if (!result) { std::cerr << "Rejected page " << start << ": " << haipower::error_name(result.error) << '\n'; return 1; }
      std::cout << "Validated page " << start << ", registers=" << result.value << '\n';
      const haipower::RegisterBlockView block = {haipower::RegisterTable::Input,
          static_cast<std::uint16_t>(start),registers,125};
      const auto& profile = haipower::aohai_modbus_v214_profile();
      for (std::size_t i = 0; i < profile.field_count; ++i) {
        const auto reading = haipower::decode_signal(profile,profile.fields[i].signal,block);
        if (reading) std::cout << haipower::signal_name(reading.value.signal) << '=' << haipower::scaled_value(reading.value)
                               << haipower::unit_symbol(reading.value.unit) << " raw=" << reading.value.raw << '\n';
      }
      ++pages;
    }
    return pages > 0 && std::cin.eof() ? 0 : 2;
  }
  if (argc != 1) return 2;
  const haipower::BleLoggerIdentity identity = {"WF12345678", 10};
  const char* vectors[] = {
    "0028000100224217263536983910439f8521afde476994d6a9bb182f8055f5536fad612a0f3b36b524d5",
    "0028000100220017e13511139f186ba2bc9322eb1b74dd7972db9e2c47926455344d5d055f3d7c14e05e",
    "002800010022011790954a55ef335ea01c7ec1a54a18be745fda4a89a4124cb816c9e12a80de2a8e2d5b",
    "0028000100220217a40ba946d3f8ef693ad21f0ba68086913a018b72a147923bb8a83a003889861dc5c2",
    "002800010022031742fe905560b4c889771490a382ed45ae2760c1e20acdaccd2d58e4d3ac0e3bc94445"};
  for (unsigned i = 0; i < 5; ++i) {
    std::uint8_t wire[haipower::kBlePageReadFrameSize] = {};
    const auto r = haipower::build_ble_page_read(identity, 1,
        i == 0 ? haipower::RegisterTable::Holding : haipower::RegisterTable::Input,
        i == 0 ? 0 : (i-1)*125, 125, i == 0 ? 0x42 : i-1,
        crypto, wire, sizeof(wire));
    const auto expected = hex(vectors[i]);
    CHECK(r && r.value == expected.size());
    CHECK(std::memcmp(wire, expected.data(), expected.size()) == 0);
  }
  // All supported name lengths, padding boundaries, exact/short capacity,
  // both read functions and count extremes; no writes/broadcast are admitted.
  const char* name = "WF12345678901234567890123456789012";
  for (std::size_t n = 1; n <= 32; ++n) {
    const std::size_t size = 10 + std::max(std::size_t(32), ((n + 25) / 16) * 16);
    std::uint8_t wire[haipower::kBlePageReadFrameSize + 1];
    std::memset(wire, 0xaa, sizeof(wire));
    auto r = haipower::build_ble_page_read({name,n}, 247,
        haipower::RegisterTable::Input, 65535, 1, 254, crypto, wire, size);
    CHECK(r && r.value == size && wire[size] == 0xaa);
    CHECK(haipower::build_ble_page_read({name,n}, 1, haipower::RegisterTable::Holding,
        0,125,0,crypto,wire,size-1).error == haipower::Error::BufferTooSmall);
  }
  std::uint8_t wire[58] = {};
  for (auto invalid : {haipower::BleLoggerIdentity{nullptr,10}, {name,0},
                      {name,33}, {"WF\n123",6}, {"WF\0ABC",6}, {"\xc3\xa9",2}})
    CHECK(haipower::build_ble_page_read(invalid,1,haipower::RegisterTable::Input,
        0,125,0,crypto,wire,sizeof(wire)).error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_ble_page_read(identity,0,haipower::RegisterTable::Input,
      0,125,0,crypto,wire,sizeof(wire)).error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_ble_page_read(identity,1,static_cast<haipower::RegisterTable>(2),
      0,125,0,crypto,wire,sizeof(wire)).error == haipower::Error::InvalidArgument);

  // Candidate response layout is synthetic, not a device observation. It must
  // reject CRC/identity/function/length mismatches before exposing registers.
  auto plain = hex("574631323334353637380007010402004d7905");
  std::uint16_t reg = 0;
  auto parse = [&]() { return haipower::parse_ble_read_plaintext(plain.data(),plain.size(),
      identity,1,haipower::RegisterTable::Input,1,&reg,1); };
  CHECK(parse() && reg == 77);
  plain.back() ^= 1; reg = 1234;
  CHECK(parse().error == haipower::Error::BadCrc && reg == 1234);
  plain.back() ^= 1; plain[0] ^= 1;
  CHECK(parse().error == haipower::Error::WrongDeviceIdentity);
  plain[0] ^= 1; plain[11] = 6;
  CHECK(parse().error == haipower::Error::BadLength);
  plain[11] = 7;
  CHECK(haipower::parse_ble_read_plaintext(plain.data(),plain.size(),identity,2,
      haipower::RegisterTable::Input,1,&reg,1).error == haipower::Error::WrongSlave);
  CHECK(haipower::parse_ble_read_plaintext(plain.data(),plain.size(),identity,1,
      haipower::RegisterTable::Holding,1,&reg,1).error == haipower::Error::WrongFunction);

  // Independently generated complete response, synthetic logger/SOC=77.
  const auto response_golden = hex("0028000100224217c32ec71a10392097f77c468a64d7a130d56b1fe2fefd0e392b84262b049245c16112");
  auto response = response_golden;
  auto parse_frame = [&]() { return haipower::parse_ble_read_frame(response.data(),response.size(),
      identity,1,haipower::RegisterTable::Input,1,0x42,crypto,&reg,1); };
  auto repair_outer_crc = [&]() {
    const auto crc = haipower::crc16_modbus(response.data(),response.size()-2);
    response[response.size()-2] = crc >> 8; response.back() = crc;
  };
  CHECK(parse_frame() && reg == 77);
  // Any single-byte corruption must fail without changing published output.
  for (std::size_t i = 0; i < response.size(); ++i) {
    response = response_golden; response[i] ^= 1; reg = 999;
    CHECK(!parse_frame() && reg == 999);
  }
  response = response_golden; response[6] = 0x43; repair_outer_crc();
  CHECK(parse_frame().error == haipower::Error::WrongCorrelation);
  response = response_golden;
  CHECK(haipower::parse_ble_read_frame(response.data(),response.size(),{"XX12345678",10},
      1,haipower::RegisterTable::Input,1,0x42,crypto,&reg,1).error == haipower::Error::WrongDeviceIdentity);
  CHECK(haipower::parse_ble_read_frame(response.data(),response.size(),identity,
      2,haipower::RegisterTable::Input,1,0x42,crypto,&reg,1).error == haipower::Error::WrongSlave);
  CHECK(haipower::parse_ble_read_frame(response.data(),response.size(),identity,
      1,haipower::RegisterTable::Holding,1,0x42,crypto,&reg,1).error == haipower::Error::WrongFunction);
  for (std::size_t size = 0; size < response.size(); ++size)
    CHECK(!haipower::parse_ble_read_frame(response.data(),size,identity,
        1,haipower::RegisterTable::Input,1,0x42,crypto,&reg,1));
  // Valid outer CRC is insufficient: corrupted inner RTU CRC must still fail.
  auto bad_inner = plain; bad_inner.back() ^= 1; bad_inner.resize(32,0);
  CHECK(encrypt(haipower::kHaiPowerAesKey,haipower::kHaiPowerAesIv,
                bad_inner.data(),bad_inner.size(),response.data()+8,nullptr));
  repair_outer_crc();
  CHECK(parse_frame().error == haipower::Error::BadCrc);

  plain.resize(32,0);
  CHECK(parse() && reg == 77);
  plain.back() = 1;
  CHECK(parse().error == haipower::Error::BadPadding);

  // Real CBC at the app's eight-byte offset. Diagnostic decryption preserves
  // zeros and does NOT interpret outer fields or claim valid packet framing.
  for (unsigned padding = 0; padding <= 16; ++padding) {
    std::uint8_t notification[40] = {0,38,0,1,0,34,7,0x17};
    std::uint8_t padded[32] = {}, decoded[32] = {};
    std::memset(padded,0xa5,32-padding);
    CHECK(encrypt(haipower::kHaiPowerAesKey,haipower::kHaiPowerAesIv,padded,32,notification+8,nullptr));
    auto r = haipower::decrypt_ble_notification(notification,40,crypto,decoded,32);
    CHECK(r && r.value == 32 && std::memcmp(decoded,padded,32) == 0);
    CHECK(haipower::decrypt_ble_notification(notification,40,crypto,decoded,31).error == haipower::Error::BufferTooSmall);
    CHECK(haipower::decrypt_ble_notification(notification,8,crypto,decoded,32).error == haipower::Error::BadLength);
    CHECK(haipower::decrypt_ble_notification(notification,505,crypto,decoded,32).error == haipower::Error::BadLength);
  }
  // Reproduce the app even when a trailer makes the ciphertext non-aligned.
  // These bytes are diagnostic only: the parser must not silently discard it.
  for (std::size_t source_size : {1U,16U,17U,31U,32U,33U,272U,274U,496U}) {
    const std::size_t padded_size = std::max(std::size_t(32), ((source_size+15)/16)*16);
    std::vector<std::uint8_t> notification(source_size+8,0x5a);
    std::vector<std::uint8_t> source(padded_size,0), expected(padded_size), actual(padded_size);
    std::copy(notification.begin()+8,notification.end(),source.begin());
    CHECK(transform(haipower::kHaiPowerAesKey,haipower::kHaiPowerAesIv,
                    source.data(),padded_size,expected.data(),nullptr));
    auto r = haipower::decrypt_ble_notification(notification.data(),notification.size(),
                                               crypto,actual.data(),actual.size());
    CHECK(r && r.value == padded_size && actual == expected);
  }
  if (failures) return 1;
  std::cout << "Independent BLE AES vectors, bounds and response validation passed\n";
}
