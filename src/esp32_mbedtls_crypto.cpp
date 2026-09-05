#include "haipower/ble_codec.h"

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)

#include <cstring>

#include <mbedtls/aes.h>

namespace haipower {
namespace {

bool transform(int mode,
               const std::uint8_t* key,
               const std::uint8_t* iv,
               const std::uint8_t* input,
               std::size_t input_size,
               std::uint8_t* output,
               void*) {
  if (key == nullptr || iv == nullptr || input == nullptr || output == nullptr ||
      input_size == 0 || (input_size % 16) != 0) {
    return false;
  }

  std::uint8_t mutable_iv[16];
  std::memcpy(mutable_iv, iv, sizeof(mutable_iv));
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const int key_result = mode == MBEDTLS_AES_ENCRYPT
                             ? mbedtls_aes_setkey_enc(&aes, key, 128)
                             : mbedtls_aes_setkey_dec(&aes, key, 128);
  const int crypt_result =
      key_result == 0
          ? mbedtls_aes_crypt_cbc(&aes, mode, input_size, mutable_iv, input,
                                  output)
          : key_result;
  mbedtls_aes_free(&aes);
  return crypt_result == 0;
}

bool encrypt(const std::uint8_t* key,
             const std::uint8_t* iv,
             const std::uint8_t* input,
             std::size_t input_size,
             std::uint8_t* output,
             void* context) {
  return transform(MBEDTLS_AES_ENCRYPT, key, iv, input, input_size, output,
                   context);
}

bool decrypt(const std::uint8_t* key,
             const std::uint8_t* iv,
             const std::uint8_t* input,
             std::size_t input_size,
             std::uint8_t* output,
             void* context) {
  return transform(MBEDTLS_AES_DECRYPT, key, iv, input, input_size, output,
                   context);
}

}  // namespace

Aes128Cbc esp32_mbedtls_aes128_cbc() {
  return {encrypt, decrypt, nullptr};
}

}  // namespace haipower

#endif
