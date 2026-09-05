#pragma once
// Deliberately minimal host double, NOT an ESP-IDF emulator or ABI proof.
// The production adapter is also compiled against real ESP-IDF headers by PIO.
#include <cstddef>
#include <cstdint>
#define ESP_IDF_VERSION_MAJOR 4
using esp_gatt_if_t = std::uint8_t;
using esp_ble_addr_type_t = int;
constexpr int BLE_ADDR_TYPE_PUBLIC=0, BLE_ADDR_TYPE_RANDOM=1;
constexpr int ESP_OK=0, ESP_GATT_OK=0, ESP_UUID_LEN_16=2;
constexpr int ESP_BT_CONTROLLER_STATUS_IDLE=0, ESP_BLUEDROID_STATUS_UNINITIALIZED=0;
constexpr int ESP_GATT_CHAR_PROP_BIT_WRITE=8, ESP_GATT_CHAR_PROP_BIT_NOTIFY=16;
constexpr int ESP_GATT_WRITE_TYPE_RSP=2, ESP_GATT_AUTH_REQ_NONE=0;
constexpr esp_gatt_if_t ESP_GATT_IF_NONE=255;
constexpr int pdTRUE=1;
enum esp_gattc_cb_event_t {
  ESP_GATTC_REG_EVT, ESP_GATTC_UNREG_EVT, ESP_GATTC_OPEN_EVT,
  ESP_GATTC_CLOSE_EVT, ESP_GATTC_DISCONNECT_EVT, ESP_GATTC_CFG_MTU_EVT,
  ESP_GATTC_SEARCH_RES_EVT, ESP_GATTC_SEARCH_CMPL_EVT, ESP_GATTC_REG_FOR_NOTIFY_EVT,
  ESP_GATTC_WRITE_DESCR_EVT, ESP_GATTC_WRITE_CHAR_EVT, ESP_GATTC_NOTIFY_EVT,
  ESP_GATTC_SRVC_CHG_EVT
};
struct esp_bt_uuid_t { std::uint16_t len; union { std::uint16_t uuid16; } uuid; };
struct esp_gattc_char_elem_t { std::uint16_t char_handle; std::uint8_t properties; };
struct esp_gattc_descr_elem_t { std::uint16_t handle; esp_bt_uuid_t uuid; };
struct esp_ble_gattc_cb_param_t {
  struct { std::uint16_t app_id, status; } reg;
  struct { std::uint16_t status, conn_id; std::uint8_t remote_bda[6]; } open;
  struct { std::uint16_t status, conn_id; } close;
  struct { std::uint16_t conn_id; std::uint8_t remote_bda[6]; } disconnect;
  struct { std::uint16_t status, conn_id, mtu; } cfg_mtu;
  struct { std::uint16_t conn_id, start_handle, end_handle;
           struct { esp_bt_uuid_t uuid; } srvc_id; } search_res;
  struct { std::uint16_t conn_id, status; } search_cmpl;
  struct { std::uint16_t status, handle; } reg_for_notify;
  struct { std::uint16_t status, conn_id, handle; } write;
  struct { std::uint16_t conn_id, handle, value_len;
           std::uint8_t remote_bda[6]; std::uint8_t* value; } notify;
  struct { std::uint8_t remote_bda[6]; } srvc_chg;
};
using Callback = void (*)(esp_gattc_cb_event_t,esp_gatt_if_t,esp_ble_gattc_cb_param_t*);
constexpr int ESP_BT_STATUS_SUCCESS=0;
constexpr int BLE_SCAN_TYPE_ACTIVE=1, BLE_SCAN_FILTER_ALLOW_ALL=0, BLE_SCAN_DUPLICATE_DISABLE=0;
constexpr int ESP_GAP_SEARCH_INQ_RES_EVT=0, ESP_GAP_SEARCH_INQ_CMPL_EVT=1;
enum esp_gap_ble_cb_event_t {
  ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT, ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,
  ESP_GAP_BLE_SCAN_RESULT_EVT, ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
};
struct esp_ble_scan_params_t {
  int scan_type, own_addr_type, scan_filter_policy;
  std::uint16_t scan_interval, scan_window;
  int scan_duplicate;
};
struct esp_ble_gap_cb_param_t {
  struct { std::uint16_t status; } scan_param_cmpl, scan_start_cmpl;
  struct {
    int search_evt;
    std::uint8_t bda[6];
    int ble_addr_type, rssi;
    std::uint8_t ble_adv[62];
    std::uint8_t adv_data_len, scan_rsp_len;
  } scan_rst;
};
using GapCallback = void (*)(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t*);
int esp_ble_gap_register_callback(GapCallback);
int esp_ble_gap_set_scan_params(esp_ble_scan_params_t*);
int esp_ble_gap_start_scanning(std::uint32_t);
int esp_ble_gap_stop_scanning();
using QueueHandle_t = void*;
QueueHandle_t xQueueCreate(unsigned, std::size_t);
int xQueueSend(QueueHandle_t,const void*,int);
int xQueueReceive(QueueHandle_t,void*,int);
std::uint32_t millis();
bool btStart();
int esp_bt_controller_get_status();
int esp_bluedroid_get_status();
int esp_bluedroid_init();
int esp_bluedroid_enable();
int esp_ble_gattc_register_callback(Callback);
int esp_ble_gatt_set_local_mtu(std::uint16_t);
int esp_ble_gattc_app_register(std::uint16_t);
int esp_ble_gattc_app_unregister(esp_gatt_if_t);
int esp_ble_gattc_open(esp_gatt_if_t,std::uint8_t*,esp_ble_addr_type_t,bool);
int esp_ble_gattc_close(esp_gatt_if_t,std::uint16_t);
int esp_ble_gap_disconnect(std::uint8_t*);
int esp_ble_gattc_send_mtu_req(esp_gatt_if_t,std::uint16_t);
int esp_ble_gattc_search_service(esp_gatt_if_t,std::uint16_t,esp_bt_uuid_t*);
int esp_ble_gattc_get_char_by_uuid(esp_gatt_if_t,std::uint16_t,std::uint16_t,
    std::uint16_t,esp_bt_uuid_t,esp_gattc_char_elem_t*,std::uint16_t*);
int esp_ble_gattc_get_descr_by_char_handle(esp_gatt_if_t,std::uint16_t,std::uint16_t,
    esp_bt_uuid_t,esp_gattc_descr_elem_t*,std::uint16_t*);
int esp_ble_gattc_register_for_notify(esp_gatt_if_t,std::uint8_t*,std::uint16_t);
int esp_ble_gattc_write_char_descr(esp_gatt_if_t,std::uint16_t,std::uint16_t,
    std::uint16_t,std::uint8_t*,int,int);
int esp_ble_gattc_write_char(esp_gatt_if_t,std::uint16_t,std::uint16_t,
    std::uint16_t,std::uint8_t*,int,int);
