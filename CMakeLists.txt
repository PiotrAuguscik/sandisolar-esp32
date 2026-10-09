cmake_minimum_required(VERSION 3.16)

set(HAIPOWER_SOURCES
    src/ble_codec.cpp
    src/ble_discovery.cpp
    src/ble_session.cpp
    src/arduino_bluedroid_link.cpp
    src/arduino_nimble_link.cpp
    src/crc16_modbus.cpp
    src/esp32_mbedtls_crypto.cpp
    src/modbus_rtu.cpp
    src/profile.cpp
    src/signal_read.cpp
    src/register_value.cpp
    src/experimental.cpp
    src/types.cpp)

if(COMMAND idf_component_register)
  idf_component_register(SRCS ${HAIPOWER_SOURCES} INCLUDE_DIRS src)
  return()
endif()

project(sandisolar_esp32 VERSION 0.6.0 LANGUAGES CXX)

add_library(haipower ${HAIPOWER_SOURCES})
add_library(sandisolar ALIAS haipower)
target_include_directories(haipower PUBLIC src)
target_compile_features(haipower PUBLIC cxx_std_11)
target_compile_options(haipower PRIVATE -Wall -Wextra -Wpedantic)

option(HAIPOWER_BUILD_TESTS "Build host-side tests" ON)
if(HAIPOWER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
