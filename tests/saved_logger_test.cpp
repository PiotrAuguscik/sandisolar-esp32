#include "../examples/ReadBatteryBle/SavedLogger.h"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "line " << __LINE__ << '\n'; std::abort(); } } while (false)
static void checksum(std::uint8_t* bytes) {
  const auto crc = sandisolar::crc16_modbus(bytes, saved_logger::size - 2);
  bytes[42] = static_cast<std::uint8_t>(crc);
  bytes[43] = static_cast<std::uint8_t>(crc >> 8);
}
int main() {
  sandisolar::BleScanCandidate input{};
  input.address[0] = 0xc2; input.address[5] = 1;
  std::memcpy(input.advertisement.name, "WF12345678", 11);
  input.advertisement.name_size = 10;
  for (auto type : {sandisolar::BleAddressType::Public, sandisolar::BleAddressType::Random}) {
    input.address_type = type;
    std::uint8_t bytes[saved_logger::size]{};
    CHECK(saved_logger::encode(input, bytes));
    sandisolar::BleScanCandidate output{};
    CHECK(saved_logger::decode(bytes, sizeof(bytes), output));
    CHECK(output.address_type == type && !std::memcmp(input.address, output.address, 6));
    CHECK(saved_logger::matches(output, "WF12345678"));
    CHECK(saved_logger::matches(output, ""));
    CHECK(!saved_logger::matches(output, "WF87654321"));
    CHECK(!saved_logger::matches(output, nullptr));
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
      for (unsigned bit = 0; bit < 8; ++bit) {
        bytes[i] ^= 1U << bit;
        CHECK(!saved_logger::decode(bytes, sizeof(bytes), output));
        CHECK(saved_logger::matches(output, "WF12345678"));
        bytes[i] ^= 1U << bit;
      }
    }
    for (unsigned length = 0; length < sizeof(bytes); ++length)
      CHECK(!saved_logger::decode(bytes, length, output));
    CHECK(!saved_logger::decode(bytes, sizeof(bytes) + 1, output));
    CHECK(!saved_logger::decode(nullptr, sizeof(bytes), output));
    for (unsigned offset : {0U, 1U, 2U, 41U}) {
      const auto original = bytes[offset];
      bytes[offset] = 255; checksum(bytes);
      CHECK(!saved_logger::decode(bytes, sizeof(bytes), output));
      bytes[offset] = original; checksum(bytes);
    }
    auto copy = output;
    output.advertisement.name[0] = 'X';
    CHECK(saved_logger::matches(copy, "WF12345678")); // owned identity, no dangling pointers
  }
  std::uint8_t bytes[saved_logger::size]{};
  input.advertisement.name_size = 33;
  CHECK(!saved_logger::encode(input, bytes));
  input.advertisement.name_size = 0;
  CHECK(!saved_logger::encode(input, bytes));
  input.advertisement.name_size = 10;
  std::memset(input.address, 0, 6); CHECK(!saved_logger::encode(input, bytes));
  std::memset(input.address, 255, 6); CHECK(!saved_logger::encode(input, bytes));
  std::cout << "Saved logger record validation passed\n";
}
