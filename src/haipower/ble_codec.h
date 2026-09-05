#pragma once

#include <cstddef>
#include <cstdint>

#include "haipower/types.h"

namespace haipower {

constexpr std::size_t kBleLoggerIdentityMaxSize = 32;
// Maximum buffer size, not the on-wire length. A ten-byte logger name makes
// a 42-byte request. Always transmit the size returned by the builder.
constexpr std::size_t kBlePageReadFrameSize = 58;
constexpr std::size_t kBleNotificationPlaintextCapacity = 496;
// Hardware-confirmed 125-register response with ten-byte logger identity.
constexpr std::size_t kBleValidatedPageNotificationSize = 282;
constexpr std::size_t kBlePageRegisterCount = 125;
constexpr std::size_t kBlePageRegisterBytes = 250;
constexpr std::uint8_t kBleMarker = 0x17;

extern const std::uint8_t kHaiPowerAesKey[16];
extern const std::uint8_t kHaiPowerAesIv[16];

using Aes128CbcTransform = bool (*)(const std::uint8_t* key,
                                    const std::uint8_t* iv,
                                    const std::uint8_t* input,
                                    std::size_t input_size,
                                    std::uint8_t* output,
                                    void* context);

struct Aes128Cbc {
  Aes128CbcTransform encrypt;
  Aes128CbcTransform decrypt;
  void* context;
};

// Borrowed, bounded ASCII logger name from the selected BLE device, NOT the
// inverter serial or MAC. Non-ASCII names are deliberately unsupported until
// the app's string adapter has been verified for them.
struct BleLoggerIdentity {
  const char* data;
  std::size_t size;
};

bool valid_ble_logger_identity(BleLoggerIdentity identity);

// HaiPower 2.0.3 local BLE request framing. Requires the selected logger's
// identity. Only FC03/FC04, count 1..125, correlation 0..254.
Result<std::size_t> build_ble_page_read(
    BleLoggerIdentity identity,
    std::uint8_t slave,
    RegisterTable table,
    std::uint16_t start,
    std::uint16_t count,
    std::uint8_t correlation,
    const Aes128Cbc& crypto,
    std::uint8_t* output,
    std::size_t output_capacity);

// Bounded diagnostic decryption mirroring the app: skip EIGHT bytes, copy
// remainder into a zero-extended CBC block buffer (minimum 32), then decrypt
// without PKCS#7. Returns the complete padded byte count, including opaque
// trailer effects. Does NOT validate outer framing, integrity, identity or
// correlation. Always retain the original frame. Output is usable only on
// success, and usable means raw evidence, NOT a validated telemetry reading.
Result<std::size_t> decrypt_ble_notification(
    const std::uint8_t* frame, std::size_t frame_size,
    const Aes128Cbc& crypto, std::uint8_t* plaintext,
    std::size_t plaintext_capacity);

// Observed content: identity || BE16(RTU length) || RTU response.
// Four FC04 pages verified on one AO-6KSL-G3. Reuses the RTU parser to
// verify slave, function, count and CRC. Accepts exact content or zero-filled
// app buffer only. Opaque trailers fail closed. Does not validate correlation.
Result<std::size_t> parse_ble_read_plaintext(
    const std::uint8_t* plaintext, std::size_t plaintext_size,
    BleLoggerIdentity identity, std::uint8_t slave, RegisterTable table,
    std::uint16_t expected_count, std::uint16_t* registers,
    std::size_t register_capacity);

// Validates the complete hardware-observed envelope, including BOTH lengths,
// protocol/marker, outer big-endian CRC and expected correlation; excludes the
// CRC trailer from CBC decryption, then validates identity/RTU/zero fill using
// parse_ble_read_plaintext. Only this API establishes a validated BLE read.
// 125-register FC04 responses verified on AO-6KSL-G3; other counts/FC03 are
// supported by the codec contract but still await peripheral qualification.
Result<std::size_t> parse_ble_read_frame(
    const std::uint8_t* frame, std::size_t frame_size,
    BleLoggerIdentity identity, std::uint8_t slave, RegisterTable table,
    std::uint16_t expected_count, std::uint8_t expected_correlation,
    const Aes128Cbc& crypto, std::uint16_t* registers,
    std::size_t register_capacity);

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
Aes128Cbc esp32_mbedtls_aes128_cbc();
#endif

}  // namespace haipower
