add_executable(haipower_tests test_main.cpp)
target_link_libraries(haipower_tests PRIVATE haipower)
target_compile_options(haipower_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME haipower_tests COMMAND haipower_tests)

add_executable(profile_fields_tests profile_fields_test.cpp)
target_link_libraries(profile_fields_tests PRIVATE haipower)
target_compile_options(profile_fields_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profile_fields_tests COMMAND profile_fields_tests)

add_executable(signal_read_tests signal_read_test.cpp)
target_link_libraries(signal_read_tests PRIVATE haipower)
target_compile_options(signal_read_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME signal_read_tests COMMAND signal_read_tests)

add_executable(experimental_tests experimental_test.cpp)
target_link_libraries(experimental_tests PRIVATE haipower)
target_compile_options(experimental_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME experimental_tests COMMAND experimental_tests)

add_executable(ble_session_tests ble_session_test.cpp)
target_link_libraries(ble_session_tests PRIVATE haipower)
target_compile_options(ble_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ble_session_tests COMMAND ble_session_tests)

# Execute the actual adapter against a bounded SDK double. This is semantic
# coverage, not radio/ESP-IDF backend qualification; the ESP example builds too.
add_executable(bluedroid_link_tests bluedroid_link_test.cpp ../src/arduino_bluedroid_link.cpp)
target_include_directories(bluedroid_link_tests PRIVATE bluedroid_shim)
target_compile_definitions(bluedroid_link_tests PRIVATE ARDUINO_ARCH_ESP32)
target_link_libraries(bluedroid_link_tests PRIVATE haipower)
target_compile_options(bluedroid_link_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME bluedroid_link_tests COMMAND bluedroid_link_tests)
foreach(scenario scan_happy scan_config_timeout scan_timeout scan_control_overflow scan_duplicate_control)
  add_test(NAME ${scenario} COMMAND bluedroid_link_tests ${scenario})
endforeach()
foreach(scenario init_wifi_absent init_wifi_null init_wifi_min init_wifi_max
    init_wifi_ap init_wifi_apsta init_wifi_none_sta init_wifi_none_ap init_wifi_none_apsta
    init_wifi_mode_error init_wifi_ps_error init_wifi_ps_unknown
    init_controller_error init_controller_stuck init_enable_error init_enable_stuck)
  add_test(NAME ${scenario} COMMAND bluedroid_link_tests ${scenario})
  set_tests_properties(${scenario} PROPERTIES TIMEOUT 5)
endforeach()

add_executable(ble_discovery_tests ble_discovery_test.cpp)
target_link_libraries(ble_discovery_tests PRIVATE haipower)
target_compile_options(ble_discovery_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ble_discovery_tests COMMAND ble_discovery_tests)

# Host tests only: independent real AES implementation. Not a library/ESP dep.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_executable(ble_openssl_tests ble_openssl_test.cpp)
target_link_libraries(ble_openssl_tests PRIVATE haipower OpenSSL::Crypto)
target_compile_options(ble_openssl_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ble_openssl_tests COMMAND ble_openssl_tests)
