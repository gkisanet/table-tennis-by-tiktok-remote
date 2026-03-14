/*
 * ESP32-S3 BLE HID Host - Digital Table Tennis Scoreboard
 * JX-05 BLE 리모컨으로 P5 LED 탁구 점수판을 제어합니다.
 *
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#define ESP_BD_ADDR_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define ESP_BD_ADDR_HEX(addr)                                                  \
  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]
#else
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#endif

#include "esp_hid_gap.h"
#include "esp_hidh.h"
#include "hub75_display.h"
#include "scoreboard.h"

static const char *TAG = "SCOREBOARD";

// ============================================
// GATTC 콜백 래퍼 (BLE 이벤트 전달)
// ============================================
#if CONFIG_BT_BLE_ENABLED
static void my_gattc_callback(esp_gattc_cb_event_t event,
                              esp_gatt_if_t gattc_if,
                              esp_ble_gattc_cb_param_t *param) {
  esp_hidh_gattc_event_handler(event, gattc_if, param);
}
#endif

// ============================================
// JX-05 리모컨 설정
// ============================================
static const uint8_t TARGET_BDA[6] = {0x6c, 0x86, 0x80, 0xf2, 0x7e, 0x7f};
static const char *TARGET_NAME = "JX-05";

// ============================================
// 버튼 정의
// ============================================
typedef enum {
  BTN_NONE = 0,
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_RIGHT,
  BTN_CENTER,
  BTN_CENTER_LONG,
} button_event_t;

static const char *btn_names[] = {
    "NONE",   "UP",    "DOWN",  "LEFT",
    "RIGHT",  "CENTER", "CENTER_LONG",
};

// 디바운스 상수
#define DEBOUNCE_TIME_MS 150

// ============================================
// 공유 상태 변수
// ============================================
static volatile bool device_connected = false;
static scoreboard_t scoreboard;
static volatile button_event_t last_button = BTN_NONE;
static volatile bool blink_state = false;

// 디바운스 상태
static int64_t last_btn_time = 0;
static button_event_t last_btn_type = BTN_NONE;

#if !CONFIG_BT_NIMBLE_ENABLED
static char *bda2str(uint8_t *bda, char *str, size_t size) {
  if (bda == NULL || str == NULL || size < 18) return NULL;
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}
#endif

// MAC 주소 비교
static bool is_target_device(const uint8_t *bda) {
  return (memcmp(bda, TARGET_BDA, 6) == 0);
}

// ============================================
// JX-05 버튼 감지 (Report ID 2, RELEASE 이벤트 절대값 기반)
// ============================================
static button_event_t detect_button_report2(const uint8_t *data, uint8_t len) {
  if (len < 8) return BTN_NONE;

  uint8_t action = data[0]; // 0x07=눌림, 0x00=뗌
  uint8_t axis   = data[1]; // 0x04=좌/우, 0x06=상/하/중간
  uint8_t state  = data[7]; // 0x01=눌림, 0x00=뗌

  // RELEASE 이벤트에서만 감지
  if (action == 0x00 && state == 0x00) {
    uint16_t x_val = (data[3] << 8) | data[2];
    uint16_t y_val = (data[5] << 8) | data[4];

    if (axis == 0x04) {
      // 좌/우 (X값 기반)
      if (x_val > 0x0700) return BTN_LEFT;
      if (x_val < 0x0200) return BTN_RIGHT;
    } else if (axis == 0x06) {
      // 상/하/중간 (Y값 기반)
      if (y_val > 0x0a00) return BTN_UP;
      if (y_val < 0x0300) return BTN_DOWN;
      return BTN_CENTER;
    }
  }

  return BTN_NONE;
}

// 디바운스 처리 후 버튼 이벤트 등록
static void register_button(button_event_t btn) {
  if (btn == BTN_NONE) return;

  int64_t now = esp_timer_get_time() / 1000;
  if (btn == last_btn_type && (now - last_btn_time) < DEBOUNCE_TIME_MS) {
    return; // 디바운스
  }

  last_btn_type = btn;
  last_btn_time = now;
  last_button = btn;

  ESP_LOGI(TAG, ">>> BUTTON: %s <<<", btn_names[btn]);
}

// ============================================
// 게임 상태 기반 버튼 처리 (참고 프로젝트와 동일)
// ============================================
static void process_button(button_event_t btn) {
  if (btn == BTN_NONE) return;

  ESP_LOGI(TAG, "Processing button: %s in state: %d", btn_names[btn], scoreboard.state);

  switch (scoreboard.state) {
  case GAME_STATE_SELECT_FIRST:
    if (btn == BTN_LEFT) {
      scoreboard.serve_side = 0;
      if (scoreboard.left_sets == 0 && scoreboard.right_sets == 0) {
        scoreboard.first_serve_side = 0;
      }
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Left player serves first");
    } else if (btn == BTN_RIGHT) {
      scoreboard.serve_side = 1;
      if (scoreboard.left_sets == 0 && scoreboard.right_sets == 0) {
        scoreboard.first_serve_side = 1;
      }
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Right player serves first");
    }
    break;

  case GAME_STATE_PLAYING:
    if (btn == BTN_LEFT) {
      scoreboard_add_point(&scoreboard, 0);
      ESP_LOGI(TAG, "Left +1: %d-%d", scoreboard.left_score, scoreboard.right_score);
    } else if (btn == BTN_RIGHT) {
      scoreboard_add_point(&scoreboard, 1);
      ESP_LOGI(TAG, "Right +1: %d-%d", scoreboard.left_score, scoreboard.right_score);
    } else if (btn == BTN_CENTER) {
      if (scoreboard_undo(&scoreboard)) {
        ESP_LOGI(TAG, "Undo: %d-%d", scoreboard.left_score, scoreboard.right_score);
      } else {
        ESP_LOGW(TAG, "Cannot undo");
      }
    } else if (btn == BTN_UP) {
      scoreboard.menu_selection = 0;
      scoreboard.state = GAME_STATE_MENU;
      ESP_LOGI(TAG, "Opening menu");
    } else if (btn == BTN_DOWN) {
      scoreboard.state = GAME_STATE_CONFIRM_RESET;
      ESP_LOGI(TAG, "Opening reset confirmation");
    }
    break;

  case GAME_STATE_CONFIRM_RESET:
    if (btn == BTN_CENTER) {
      scoreboard_reset(&scoreboard);
      ESP_LOGI(TAG, "Game reset!");
    } else if (btn == BTN_DOWN) {
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Reset cancelled");
    }
    break;

  case GAME_STATE_MENU:
    if (btn == BTN_UP) {
      scoreboard.menu_selection = 0; // Switch
    } else if (btn == BTN_DOWN) {
      scoreboard.menu_selection = 1; // Stop
    } else if (btn == BTN_CENTER) {
      if (scoreboard.menu_selection == 0) {
        scoreboard_switch_serve(&scoreboard);
        scoreboard.state = GAME_STATE_PLAYING;
        ESP_LOGI(TAG, "Serve switched to: %d", scoreboard.serve_side);
      } else {
        int winner = (scoreboard.left_score > scoreboard.right_score) ? 0 : 1;
        scoreboard_force_end_set(&scoreboard, winner);
        ESP_LOGI(TAG, "Set force ended, winner: %d", winner);
      }
    } else if (btn == BTN_LEFT || btn == BTN_RIGHT) {
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Menu cancelled");
    }
    break;

  case GAME_STATE_WINNER:
    if (btn == BTN_CENTER) {
      // 다음 세트 - 선수 위치 교체
      int temp_sets = scoreboard.left_sets;
      scoreboard.left_sets = scoreboard.right_sets;
      scoreboard.right_sets = temp_sets;

      scoreboard.left_score = 0;
      scoreboard.right_score = 0;
      scoreboard.undo_count = MAX_UNDO;
      scoreboard.history_index = 0;
      scoreboard.winner_side = -1;
      scoreboard.serve_side = scoreboard.first_serve_side;
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Next set started, sides swapped");
    }
    break;

  case GAME_STATE_MATCH_END:
    if (btn == BTN_DOWN) {
      scoreboard_reset(&scoreboard);
      ESP_LOGI(TAG, "Match reset!");
    }
    break;

  default:
    break;
  }
}

// ============================================
// BLE HID 콜백 (연결/입력/연결해제)
// ============================================
void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id,
                   void *event_data) {
  esp_hidh_event_t event = (esp_hidh_event_t)id;
  esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

  switch (event) {
  case ESP_HIDH_OPEN_EVENT: {
    if (param->open.status == ESP_OK) {
      const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
      if (bda) {
        ESP_LOGI(TAG, "=== DEVICE CONNECTED: %s ===",
                 esp_hidh_dev_name_get(param->open.dev));
        esp_hidh_dev_dump(param->open.dev, stdout);
        device_connected = true;

        // 게임 상태 전환
        if (scoreboard.state == GAME_STATE_CONNECTING) {
          scoreboard.state = GAME_STATE_SELECT_FIRST;
        } else if (scoreboard.state == GAME_STATE_DISCONNECTED) {
          // 재연결: 진행 중인 게임이 있으면 복원
          if (scoreboard.left_score > 0 || scoreboard.right_score > 0 ||
              scoreboard.left_sets > 0 || scoreboard.right_sets > 0) {
            scoreboard.state = GAME_STATE_PLAYING;
          } else {
            scoreboard.state = GAME_STATE_SELECT_FIRST;
          }
        }
      }
    } else {
      ESP_LOGE(TAG, "OPEN failed! Status: %d", param->open.status);
      device_connected = false;
    }
    break;
  }
  case ESP_HIDH_BATTERY_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
    if (bda) {
      ESP_LOGI(TAG, "BATTERY: %d%%", param->battery.level);
    }
    break;
  }
  case ESP_HIDH_INPUT_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
    if (bda) {
      // Report ID에 따른 버튼 감지
      if (param->input.report_id == 2) {
        button_event_t btn = detect_button_report2(param->input.data, param->input.length);
        register_button(btn);
      } else if (param->input.report_id == 3) {
        // Consumer Control (중간 길게)
        if (param->input.data[0] != 0x00 || param->input.data[1] != 0x00) {
          register_button(BTN_CENTER_LONG);
        }
      }
    }
    break;
  }
  case ESP_HIDH_CLOSE_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
    if (bda) {
      ESP_LOGW(TAG, "=== DEVICE DISCONNECTED ===");
    }
    device_connected = false;

    // 연결 끊김 상태로 전환 (점수는 유지)
    if (scoreboard.state != GAME_STATE_CONNECTING) {
      scoreboard.state = GAME_STATE_DISCONNECTED;
    }
    break;
  }
  default:
    break;
  }
}

// ============================================
// UI 업데이트 태스크 (LED 디스플레이 제어)
// ============================================
void ui_task(void *pvParameters) {
  TickType_t last_blink_time = xTaskGetTickCount();
  const TickType_t blink_period = pdMS_TO_TICKS(500);

  while (1) {
    // 500ms마다 깜빡임 상태 토글
    if ((xTaskGetTickCount() - last_blink_time) >= blink_period) {
      blink_state = !blink_state;
      last_blink_time = xTaskGetTickCount();
    }

    // 버튼 이벤트 처리
    button_event_t btn = last_button;
    if (btn != BTN_NONE) {
      last_button = BTN_NONE;
      process_button(btn);
    }

    // 현재 상태에 따른 화면 표시
    switch (scoreboard.state) {
    case GAME_STATE_CONNECTING:
      hub75_show_connecting();
      break;

    case GAME_STATE_DISCONNECTED:
      hub75_set_brightness(10); // 연결 끊김: 10% 밝기
      hub75_show_scoreboard(scoreboard.left_score, scoreboard.right_score,
                            scoreboard.left_sets, scoreboard.right_sets,
                            scoreboard.serve_side, blink_state, true);
      break;

    case GAME_STATE_SELECT_FIRST:
      hub75_set_brightness(20);
      hub75_show_select_first(blink_state, -1);
      break;

    case GAME_STATE_PLAYING:
      hub75_set_brightness(20);
      hub75_show_scoreboard(scoreboard.left_score, scoreboard.right_score,
                            scoreboard.left_sets, scoreboard.right_sets,
                            scoreboard.serve_side, true, false);
      break;

    case GAME_STATE_CONFIRM_RESET:
      hub75_show_confirm_reset();
      break;

    case GAME_STATE_MENU:
      hub75_show_menu(scoreboard.menu_selection);
      break;

    case GAME_STATE_WINNER:
      hub75_show_winner(scoreboard.winner_side, blink_state,
                        scoreboard.left_sets, scoreboard.right_sets,
                        scoreboard.left_score, scoreboard.right_score);
      break;

    case GAME_STATE_MATCH_END:
      hub75_show_match_end(scoreboard.winner_side, scoreboard.left_sets,
                           scoreboard.right_sets, blink_state,
                           scoreboard.left_score, scoreboard.right_score);
      break;

    default:
      hub75_show_connecting();
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz UI 갱신
  }
}

// ============================================
// BLE 스캔 및 연결 태스크
// ============================================
#define SCAN_DURATION_SECONDS 3

void hid_demo_task(void *pvParameters) {
  size_t results_len = 0;
  esp_hid_scan_result_t *results = NULL;

  ESP_LOGI(TAG, "=== Digital Table Tennis Scoreboard (BLE) ===");
  ESP_LOGI(TAG, "Target: %s", TARGET_NAME);

  while (1) {
    if (device_connected) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    ESP_LOGI(TAG, "Scanning for %s...", TARGET_NAME);
    results_len = 0;
    results = NULL;

    esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);

    if (results_len) {
      esp_hid_scan_result_t *r = results;

      while (r) {
        bool is_target = is_target_device(r->bda);
        if (!is_target && r->name != NULL) {
          is_target = (strcmp(r->name, TARGET_NAME) == 0);
        }

        if (is_target && r->transport == ESP_HID_TRANSPORT_BLE) {
          ESP_LOGI(TAG, "=== TARGET FOUND! Connecting... ===");

#if CONFIG_BT_BLE_ENABLED
          esp_hidh_dev_open(r->bda, r->transport, r->ble.addr_type);
#else
          esp_hidh_dev_open(r->bda, r->transport, 0);
#endif
          esp_hid_scan_results_free(results);
          vTaskDelay(pdMS_TO_TICKS(2000));
          goto next_scan;
        }

        r = r->next;
      }

      esp_hid_scan_results_free(results);
    }

  next_scan:
    if (!device_connected) {
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

#if CONFIG_BT_NIMBLE_ENABLED
void ble_hid_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif

// ============================================
// app_main
// ============================================
void app_main(void) {
  esp_err_t ret;
#if HID_HOST_MODE == HIDH_IDLE_MODE
  ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
  return;
#endif
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 점수판 초기화
  scoreboard_init(&scoreboard);

  // P5 LED 디스플레이 초기화
  ESP_ERROR_CHECK(hub75_init());
  hub75_set_brightness(20);
  hub75_start_refresh_task();
  hub75_show_connecting();
  ESP_LOGI(TAG, "LED display initialized (20%% brightness)");

  // BLE GAP 초기화
  ESP_LOGI(TAG, "Setting HID GAP, mode: %d", HID_HOST_MODE);
  ESP_ERROR_CHECK(esp_hid_gap_init(HID_HOST_MODE));

#if CONFIG_BT_BLE_ENABLED
  // GATTC 콜백 등록
  ESP_ERROR_CHECK(esp_ble_gattc_register_callback(my_gattc_callback));

  // BLE Bonding 설정
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t oob_support = ESP_BLE_OOB_DISABLE;

  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
  ESP_LOGI(TAG, "BLE security configured (bonding enabled)");
#endif

  // HID Host 초기화
  esp_hidh_config_t config = {
      .callback = hidh_callback,
      .event_stack_size = 4096,
      .callback_arg = NULL,
  };
  ESP_ERROR_CHECK(esp_hidh_init(&config));

#if !CONFIG_BT_NIMBLE_ENABLED
  char bda_str[18] = {0};
  ESP_LOGI(TAG, "Own address: [%s]",
           bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
#endif

#if CONFIG_BT_NIMBLE_ENABLED
  ble_store_config_init();
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ret = esp_nimble_enable(ble_hid_host_task);
  if (ret) {
    ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
  }
  vTaskDelay(200);
#endif

  // 태스크 생성
  xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL);
  xTaskCreate(&ui_task, "ui_task", 4 * 1024, NULL, 3, NULL);
}