#pragma once
using wifi_mode_t = int;
using wifi_ps_type_t = int;
constexpr int WIFI_MODE_NULL=0, WIFI_PS_NONE=0, WIFI_PS_MIN_MODEM=1, WIFI_PS_MAX_MODEM=2;
constexpr int ESP_OK=0, ESP_ERR_WIFI_NOT_INIT=1;
extern int test_ps;
inline int esp_wifi_get_mode(int* mode) { *mode=1; return ESP_OK; }
inline int esp_wifi_get_ps(int* ps) { *ps=test_ps; return ESP_OK; }
