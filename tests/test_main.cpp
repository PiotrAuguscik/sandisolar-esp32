#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "haipower.h"

namespace {

int failures = 0;

struct TestCryptoContext {
  bool fail;
  bool saw_expected_plaintext;
};

const std::uint8_t kExpectedBlePlaintext[] = {
    0x57, 0x46, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x00, 0x08, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x7d, 0x85, 0xeb, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedBleCiphertext[] = {
    0x26, 0x35, 0x36, 0x98, 0x39, 0x10, 0x43, 0x9f,
    0x85, 0x21, 0xaf, 0xde, 0x47, 0x69, 0x94, 0xd6,
    0xa9, 0xbb, 0x18, 0x2f, 0x80, 0x55, 0xf5, 0x53,
    0x6f, 0xad, 0x61, 0x2a, 0x0f, 0x3b, 0x36, 0xb5};

bool known_vector_encrypt(const std::uint8_t* key,
                          const std::uint8_t* iv,
                          const std::uint8_t* input,
                          std::size_t input_size,
                          std::uint8_t* output,
                          void* opaque) {
  TestCryptoContext* context = static_cast<TestCryptoContext*>(opaque);
  if (context == nullptr || context->fail || input_size != 32) {
    return false;
  }
  context->saw_expected_plaintext =
      std::memcmp(key, haipower::kHaiPowerAesKey, 16) == 0 &&
      std::memcmp(iv, haipower::kHaiPowerAesIv, 16) == 0 &&
      std::memcmp(input, kExpectedBlePlaintext, sizeof(kExpectedBlePlaintext)) ==
          0;
  std::memcpy(output, kExpectedBleCiphertext, sizeof(kExpectedBleCiphertext));
  return true;
}

bool copy_transform(const std::uint8_t*,
                    const std::uint8_t*,
                    const std::uint8_t* input,
                    std::size_t input_size,
                    std::uint8_t* output,
                    void* opaque) {
  TestCryptoContext* context = static_cast<TestCryptoContext*>(opaque);
  if (context == nullptr || context->fail) {
    return false;
  }
  std::memcpy(output, input, input_size);
  return true;
}

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: "           \
                << #condition << '\n';                                          \
      ++failures;                                                               \
    }                                                                           \
  } while (false)

void test_crc_and_soc_request() {
  const std::uint8_t core[] = {0x01, 0x04, 0x00, 0x80, 0x00, 0x01};
  CHECK(haipower::crc16_modbus(core, sizeof(core)) == 0x2230);

  std::uint8_t request[haipower::kReadRequestSize] = {};
  const auto result = haipower::build_read_request(
      1, haipower::RegisterTable::Input, 128, 1, request, sizeof(request));
  const std::uint8_t expected[] = {0x01, 0x04, 0x00, 0x80,
                                   0x00, 0x01, 0x30, 0x22};
  CHECK(result);
  CHECK(result.value == sizeof(expected));
  CHECK(std::memcmp(request, expected, sizeof(expected)) == 0);
}

void test_request_validation() {
  std::uint8_t request[8] = {};
  CHECK(haipower::build_read_request(0, haipower::RegisterTable::Input, 0, 1,
                                     request, sizeof(request))
            .error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_read_request(1, haipower::RegisterTable::Input, 0, 126,
                                     request, sizeof(request))
            .error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_read_request(1, haipower::RegisterTable::Input, 0xffff,
                                     2, request, sizeof(request))
            .error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_read_request(1, haipower::RegisterTable::Input, 0, 1,
                                     request, 7)
            .error == haipower::Error::BufferTooSmall);
}

void test_parse_soc_response() {
  const std::uint8_t response[] = {0x01, 0x04, 0x02, 0x00,
                                   0x4d, 0x79, 0x05};
  std::uint16_t registers[1] = {};
  const auto result = haipower::parse_read_response(
      response, sizeof(response), 1, haipower::RegisterTable::Input, 1,
      registers, 1);
  CHECK(result);
  CHECK(result.value == 1);
  CHECK(registers[0] == 77);

  std::uint8_t corrupt[sizeof(response)] = {};
  std::memcpy(corrupt, response, sizeof(response));
  corrupt[4] ^= 1;
  CHECK(haipower::parse_read_response(
            corrupt, sizeof(corrupt), 1, haipower::RegisterTable::Input, 1,
            registers, 1)
            .error == haipower::Error::BadCrc);
}

void test_response_identity_and_length() {
  const std::uint8_t response[] = {0x01, 0x04, 0x02, 0x00,
                                   0x4d, 0x79, 0x05};
  std::uint16_t registers[2] = {};
  CHECK(haipower::parse_read_response(
            response, sizeof(response), 2, haipower::RegisterTable::Input, 1,
            registers, 2)
            .error == haipower::Error::WrongSlave);
  CHECK(haipower::parse_read_response(
            response, sizeof(response), 1, haipower::RegisterTable::Holding, 1,
            registers, 2)
            .error == haipower::Error::WrongFunction);
  CHECK(haipower::parse_read_response(
            response, sizeof(response), 1, haipower::RegisterTable::Input, 2,
            registers, 2)
            .error == haipower::Error::BadLength);
  CHECK(haipower::parse_read_response(
            response, sizeof(response), 1, haipower::RegisterTable::Input, 1,
            registers, 0)
            .error == haipower::Error::BufferTooSmall);
}

void test_device_exception() {
  const std::uint8_t response[] = {0x01, 0x84, 0x02, 0xc2, 0xc1};
  std::uint16_t registers[1] = {};
  CHECK(haipower::parse_read_response(
            response, sizeof(response), 1, haipower::RegisterTable::Input, 1,
            registers, 1)
            .error == haipower::Error::DeviceException);
}

void test_battery_profile() {
  std::uint16_t values[18] = {};
  values[127 - 125] = 520;
  values[128 - 125] = 77;
  values[141 - 125] = 0xff9c;  // -100 -> -1.00 A
  values[142 - 125] = 250;     // 25.0 C
  const haipower::RegisterBlockView block = {
      haipower::RegisterTable::Input, 125, values, 18};
  const auto& profile = haipower::aohai_modbus_v214_profile();

  const auto voltage =
      haipower::decode_signal(profile, haipower::SignalId::BatteryVoltage, block);
  CHECK(voltage);
  CHECK(voltage.value.raw == 520);
  CHECK(std::abs(haipower::scaled_value(voltage.value) - 52.0) < 0.0001);

  const auto soc =
      haipower::decode_signal(profile, haipower::SignalId::BatterySoc, block);
  CHECK(soc);
  CHECK(soc.value.raw == 77);
  CHECK(soc.value.quality == haipower::Quality::UnverifiedScale);

  const auto current =
      haipower::decode_signal(profile, haipower::SignalId::BatteryCurrent, block);
  CHECK(current);
  CHECK(current.value.raw == -100);
  CHECK(std::abs(haipower::scaled_value(current.value) + 1.0) < 0.0001);

  const auto temperature = haipower::decode_signal(
      profile, haipower::SignalId::BatteryTemperature, block);
  CHECK(temperature);
  CHECK(std::abs(haipower::scaled_value(temperature.value) - 25.0) < 0.0001);
}

void test_32_bit_profile_and_bounds() {
  const std::uint16_t values[] = {0x0001, 0xe240};  // 123456 -> 12345.6 W
  const haipower::RegisterBlockView block = {
      haipower::RegisterTable::Input, 250, values, 2};
  const auto& profile = haipower::aohai_modbus_v214_profile();
  const auto power =
      haipower::decode_signal(profile, haipower::SignalId::PvTotalPower, block);
  CHECK(power);
  CHECK(power.value.raw == 123456);
  CHECK(std::abs(haipower::scaled_value(power.value) - 12345.6) < 0.0001);

  CHECK(haipower::decode_signal(profile, haipower::SignalId::BatterySoc, block)
            .error == haipower::Error::FieldOutOfRange);
}

void test_signed_32_bit_profile() {
  const std::uint16_t values[] = {0xffff, 0xff9c};  // -100 -> -10.0 W
  const haipower::RegisterBlockView block = {
      haipower::RegisterTable::Input, 286, values, 2};
  const auto power = haipower::decode_signal(
      haipower::aohai_modbus_v214_profile(),
      haipower::SignalId::ActiveOutputPower, block);
  CHECK(power);
  CHECK(power.value.raw == -100);
  CHECK(std::abs(haipower::scaled_value(power.value) + 10.0) < 0.0001);
}

void test_ble_page_read_golden_vector() {
  TestCryptoContext context = {false, false};
  const haipower::Aes128Cbc crypto = {
      known_vector_encrypt, copy_transform, &context};
  std::uint8_t frame[haipower::kBlePageReadFrameSize] = {};
  const auto result = haipower::build_ble_page_read(
      {"WF12345678", 10},
      1, haipower::RegisterTable::Holding, 0, 125, 0x42, crypto, frame,
      sizeof(frame));
  const std::uint8_t expected[] = {
      0x00, 0x28, 0x00, 0x01, 0x00, 0x22, 0x42, 0x17,
      0x26, 0x35, 0x36, 0x98, 0x39, 0x10, 0x43, 0x9f,
      0x85, 0x21, 0xaf, 0xde, 0x47, 0x69, 0x94, 0xd6,
      0xa9, 0xbb, 0x18, 0x2f, 0x80, 0x55, 0xf5, 0x53,
      0x6f, 0xad, 0x61, 0x2a, 0x0f, 0x3b, 0x36, 0xb5, 0x24, 0xd5};
  CHECK(result);
  CHECK(result.value == sizeof(expected));
  CHECK(context.saw_expected_plaintext);
  CHECK(std::memcmp(frame, expected, sizeof(expected)) == 0);
}

void test_ble_page_read_validation() {
  TestCryptoContext context = {false, false};
  const haipower::Aes128Cbc crypto = {
      known_vector_encrypt, copy_transform, &context};
  std::uint8_t frame[haipower::kBlePageReadFrameSize] = {};
  CHECK(haipower::build_ble_page_read(
            {"WF12345678", 10},
            1, haipower::RegisterTable::Holding, 0, 125, 0xff, crypto,
            frame, sizeof(frame))
            .error == haipower::Error::InvalidArgument);
  context.saw_expected_plaintext = false;
  CHECK(haipower::build_ble_page_read(
      {"WF12345678", 10},
      1, haipower::RegisterTable::Holding, 0, 1, 0x42, crypto, frame,
      sizeof(frame)));
  CHECK(!context.saw_expected_plaintext);
  CHECK(haipower::build_ble_page_read(
            {"WF12345678", 10},
            1, haipower::RegisterTable::Holding, 0, 0, 0x42, crypto, frame,
            sizeof(frame))
            .error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_ble_page_read(
            {"WF12345678", 10},
            1, haipower::RegisterTable::Holding, 0, 126, 0x42, crypto, frame,
            sizeof(frame))
            .error == haipower::Error::InvalidArgument);
  CHECK(haipower::build_ble_page_read(
            {"WF12345678", 10},
            1, haipower::RegisterTable::Holding, 0, 125, 0x42, crypto,
            frame, 41)
            .error == haipower::Error::BufferTooSmall);
  context.fail = true;
  CHECK(haipower::build_ble_page_read(
            {"WF12345678", 10},
            1, haipower::RegisterTable::Holding, 0, 125, 0x42, crypto,
            frame, sizeof(frame))
            .error == haipower::Error::CryptoFailure);
}

void make_test_notification(std::uint8_t* frame) {
  // Historical invalid shape is retained only as a REJECTION fixture.
  std::memset(frame, 0, 276);
  frame[0] = 0x01;
  frame[1] = 0x12;
  frame[2] = 0x42;
  frame[3] = haipower::kBleMarker;
  for (std::size_t index = 0; index < 15;
       ++index) {
    frame[4 + index] = static_cast<std::uint8_t>(0xa0 + index);
  }
  for (std::size_t index = 0; index < haipower::kBlePageRegisterBytes;
       ++index) {
    frame[4 + 15 + index] =
        static_cast<std::uint8_t>(index);
  }
  std::memset(frame + 4 + 15 +
                  haipower::kBlePageRegisterBytes,
              0x07, 7);
}

void test_ble_rejects_obsolete_shape() {
  TestCryptoContext context = {false, false};
  const haipower::Aes128Cbc crypto = {
      copy_transform, copy_transform, &context};
  std::uint8_t frame[276] = {};
  make_test_notification(frame);
  std::uint16_t registers[haipower::kBlePageRegisterCount] = {};
  const auto result = haipower::parse_ble_read_frame(
      frame, sizeof(frame), {"WF12345678",10}, 1,
      haipower::RegisterTable::Input, 125, 0x42, crypto, registers,
      haipower::kBlePageRegisterCount);
  CHECK(!result);
  for (auto word : registers) CHECK(word == 0);
}

}  // namespace

int main() {
  test_crc_and_soc_request();
  test_request_validation();
  test_parse_soc_response();
  test_response_identity_and_length();
  test_device_exception();
  test_battery_profile();
  test_32_bit_profile_and_bounds();
  test_signed_32_bit_profile();
  test_ble_page_read_golden_vector();
  test_ble_page_read_validation();
  test_ble_rejects_obsolete_shape();

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "All HaiPower tests passed\n";
  return 0;
}
