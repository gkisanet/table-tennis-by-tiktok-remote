/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"

#include "esp_hid_gap.h"
#include "esp_hidh.h"
#include "hub75_display.h"
#include "scoreboard.h"

static const char *TAG = "SCOREBOARD";

// Target MAC address: 12:22:34:01:00:c0
static const uint8_t TARGET_MAC[6] = {0x12, 0x22, 0x34, 0x01, 0x00, 0xc0};

// BT Connection state enum (internal to BT handling)
typedef enum {
  BT_STATE_DISCONNECTED, // Not connected, need to scan
  BT_STATE_CONNECTING,   // Connection in progress, wait for result
  BT_STATE_CONNECTED,    // Connected successfully
} bt_connection_state_t;

static volatile bt_connection_state_t bt_conn_state = BT_STATE_DISCONNECTED;

// Cached device info for reconnection
static esp_bd_addr_t cached_bda;
static esp_hid_transport_t cached_transport;
static volatile bool has_cached_device = false;

// Button state tracking for debounce (ignore repeated events)
static volatile bool button_pressed = false;
static volatile bool waiting_for_direction = false;
static volatile uint16_t first_y_coord = 0;

// Global scoreboard state
static scoreboard_t scoreboard;

// Blink state for UI (toggled by timer task)
static volatile bool blink_state = false;

// Button event types
typedef enum {
  BTN_NONE = 0,
  BTN_LEFT,
  BTN_RIGHT,
  BTN_UP,
  BTN_DOWN,
  BTN_CENTER
} button_event_t;

// Last detected button (set by callback, consumed by UI task)
static volatile button_event_t last_button = BTN_NONE;

static char *bda2str(uint8_t *bda, char *str, size_t size) {
  if (bda == NULL || str == NULL || size < 18) {
    return NULL;
  }

  uint8_t *p = bda;
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4],
          p[5]);
  return str;
}

// Check if MAC address matches target
static bool is_target_device(const uint8_t *bda) {
  return (memcmp(bda, TARGET_MAC, sizeof(TARGET_MAC)) == 0);
}

// Process button press based on current game state
static void process_button(button_event_t btn) {
  if (btn == BTN_NONE)
    return;

  ESP_LOGI(TAG, "Processing button: %d in state: %d", btn, scoreboard.state);

  switch (scoreboard.state) {
  case GAME_STATE_SELECT_FIRST:
    if (btn == BTN_LEFT) {
      scoreboard.serve_side = 0;
      // Save first serve side for subsequent sets (only if first set)
      if (scoreboard.left_sets == 0 && scoreboard.right_sets == 0) {
        scoreboard.first_serve_side = 0;
      }
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Left player serves first");
    } else if (btn == BTN_RIGHT) {
      scoreboard.serve_side = 1;
      // Save first serve side for subsequent sets (only if first set)
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
      ESP_LOGI(TAG, "Left +1: %d-%d", scoreboard.left_score,
               scoreboard.right_score);
    } else if (btn == BTN_RIGHT) {
      scoreboard_add_point(&scoreboard, 1);
      ESP_LOGI(TAG, "Right +1: %d-%d", scoreboard.left_score,
               scoreboard.right_score);
    } else if (btn == BTN_CENTER) {
      if (scoreboard_undo(&scoreboard)) {
        ESP_LOGI(TAG, "Undo: %d-%d (remaining: %d)", scoreboard.left_score,
                 scoreboard.right_score, scoreboard.undo_count);
      } else {
        ESP_LOGW(TAG, "Cannot undo (no history or limit reached)");
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
        // Switch serve
        scoreboard_switch_serve(&scoreboard);
        scoreboard.state = GAME_STATE_PLAYING;
        ESP_LOGI(TAG, "Serve switched to: %d", scoreboard.serve_side);
      } else {
        // Force end set - current leader wins
        int winner = (scoreboard.left_score > scoreboard.right_score) ? 0 : 1;
        scoreboard_force_end_set(&scoreboard, winner);
        ESP_LOGI(TAG, "Set force ended, winner: %d", winner);
      }
    } else if (btn == BTN_LEFT || btn == BTN_RIGHT) {
      // Cancel menu with LEFT or RIGHT
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Menu cancelled");
    }
    break;

  case GAME_STATE_WINNER:
    if (btn == BTN_CENTER) {
      // Next set - players swap sides (court change)
      // Swap set scores (because left player is now on right, and vice versa)
      int temp_sets = scoreboard.left_sets;
      scoreboard.left_sets = scoreboard.right_sets;
      scoreboard.right_sets = temp_sets;

      // Reset game scores
      scoreboard.left_score = 0;
      scoreboard.right_score = 0;
      scoreboard.undo_count = MAX_UNDO;
      scoreboard.history_index = 0;
      scoreboard.winner_side = -1;

      // Serve starts from the same side as first set (first_serve_side
      // unchanged)
      scoreboard.serve_side = scoreboard.first_serve_side;
      scoreboard.state = GAME_STATE_PLAYING;
      ESP_LOGI(TAG, "Next set started, sides swapped, serve: %d",
               scoreboard.serve_side);
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

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id,
                   void *event_data) {
  esp_hidh_event_t event = (esp_hidh_event_t)id;
  esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

  switch (event) {
  case ESP_HIDH_OPEN_EVENT: {
    if (param->open.status == ESP_OK) {
      const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
      if (bda) {
        ESP_LOGI(TAG, "===========================================");
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CONNECTED: %s", ESP_BD_ADDR_HEX(bda),
                 esp_hidh_dev_name_get(param->open.dev));
        ESP_LOGI(TAG, "===========================================");
        esp_hidh_dev_dump(param->open.dev, stdout);
        bt_conn_state = BT_STATE_CONNECTED;

        // Update game state
        if (scoreboard.state == GAME_STATE_CONNECTING) {
          scoreboard.state = GAME_STATE_SELECT_FIRST;
        } else if (scoreboard.state == GAME_STATE_DISCONNECTED) {
          // Reconnected - restore to previous meaningful state (PLAYING or
          // SELECT_FIRST)
          if (scoreboard.left_score > 0 || scoreboard.right_score > 0 ||
              scoreboard.left_sets > 0 || scoreboard.right_sets > 0) {
            scoreboard.state = GAME_STATE_PLAYING;
          } else {
            scoreboard.state = GAME_STATE_SELECT_FIRST;
          }
        }
      }
    } else {
      ESP_LOGE(TAG, "OPEN failed! Will retry connection...");
      bt_conn_state = BT_STATE_DISCONNECTED;
    }
    break;
  }
  case ESP_HIDH_BATTERY_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
    if (bda) {
      ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda),
               param->battery.level);
    }
    break;
  }
  case ESP_HIDH_INPUT_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
    if (bda && param->input.length >= 6) {
      const uint8_t *data = param->input.data;
      uint8_t state = data[0];     // 0x07=pressed, 0x00=released
      uint8_t button_id = data[1]; // Button identifier
      uint16_t y_coord =
          data[4] | (data[5] << 8); // Y coordinate (little-endian)

      // Only process press events (state == 0x07)
      if (state == 0x07) {
        // Handle UP/DOWN button (0x06) with direction detection
        if (button_id == 0x06) {
          if (!waiting_for_direction && !button_pressed) {
            first_y_coord = y_coord;
            waiting_for_direction = true;
            break;
          } else if (waiting_for_direction && !button_pressed) {
            waiting_for_direction = false;
            button_pressed = true;

            if (y_coord > first_y_coord) {
              last_button = BTN_UP;
              ESP_LOGI(TAG, ">>> BUTTON: UP <<<");
            } else if (y_coord < first_y_coord) {
              last_button = BTN_DOWN;
              ESP_LOGI(TAG, ">>> BUTTON: DOWN <<<");
            }
            break;
          } else {
            break;
          }
        }

        if (button_pressed) {
          break;
        }
        button_pressed = true;

        switch (button_id) {
        case 0x04:
          last_button = BTN_LEFT;
          ESP_LOGI(TAG, ">>> BUTTON: LEFT <<<");
          break;
        case 0x05:
          last_button = BTN_RIGHT;
          ESP_LOGI(TAG, ">>> BUTTON: RIGHT <<<");
          break;
        case 0x07:
          last_button = BTN_CENTER;
          ESP_LOGI(TAG, ">>> BUTTON: CENTER <<<");
          break;
        default:
          break;
        }
      } else if (state == 0x00) {
        button_pressed = false;
        waiting_for_direction = false;
        first_y_coord = 0;
      }
    }
    break;
  }
  case ESP_HIDH_FEATURE_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
    if (bda) {
      ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d",
               ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->feature.usage),
               param->feature.map_index, param->feature.report_id,
               param->feature.length);
      ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
    }
    break;
  }
  case ESP_HIDH_CLOSE_EVENT: {
    const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
    if (bda) {
      ESP_LOGW(TAG, "===========================================");
      ESP_LOGW(TAG, ESP_BD_ADDR_STR " DISCONNECTED: %s", ESP_BD_ADDR_HEX(bda),
               esp_hidh_dev_name_get(param->close.dev));
      ESP_LOGW(TAG, "Device may be in sleep mode. Will try to reconnect...");
      ESP_LOGW(TAG, "===========================================");
    }
    bt_conn_state = BT_STATE_DISCONNECTED;

    // Don't reset scores on disconnect - mark as disconnected for UI
    if (scoreboard.state != GAME_STATE_CONNECTING) {
      scoreboard.state = GAME_STATE_DISCONNECTED;
    }
    break;
  }
  default:
    ESP_LOGI(TAG, "EVENT: %d", event);
    break;
  }
}

#define SCAN_DURATION_SECONDS 5
#define SCAN_RETRY_DELAY_MS 3000
#define RECONNECT_DELAY_MS 5000
#define CONNECTION_TIMEOUT_MS 30000

void hid_demo_task(void *pvParameters) {
  size_t results_len = 0;
  esp_hid_scan_result_t *results = NULL;
  esp_hid_scan_result_t *target_result = NULL;
  bool device_found = false;
  char bda_str[18];
  uint32_t connection_wait_time = 0;

  ESP_LOGI(TAG, "===========================================");
  ESP_LOGI(TAG, "Searching for device with MAC: %s",
           bda2str((uint8_t *)TARGET_MAC, bda_str, sizeof(bda_str)));
  ESP_LOGI(TAG, "===========================================");

  while (1) {
    switch (bt_conn_state) {
    case BT_STATE_CONNECTED:
      vTaskDelay(pdMS_TO_TICKS(1000));
      break;

    case BT_STATE_CONNECTING:
      ESP_LOGI(TAG, "Waiting for connection result...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      connection_wait_time += 1000;

      if (connection_wait_time >= CONNECTION_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Connection timeout! Will retry...");
        bt_conn_state = BT_STATE_DISCONNECTED;
        connection_wait_time = 0;
      }
      break;

    case BT_STATE_DISCONNECTED:
      connection_wait_time = 0;

      if (has_cached_device) {
        ESP_LOGI(TAG, "Attempting to reconnect to cached device...");
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));

        bt_conn_state = BT_STATE_CONNECTING;
        esp_hidh_dev_open(cached_bda, cached_transport, 0);
        break;
      }

      device_found = false;
      target_result = NULL;

      ESP_LOGI(TAG, "SCANNING for HID devices...");
      esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
      ESP_LOGI(TAG, "SCAN complete: %u devices found", results_len);

      if (results_len > 0) {
        esp_hid_scan_result_t *r = results;

        while (r) {
          printf("  %s: " ESP_BD_ADDR_STR ", ",
                 (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ",
                 ESP_BD_ADDR_HEX(r->bda));
          printf("RSSI: %d, ", r->rssi);
          printf("USAGE: %s, ", esp_hid_usage_str(r->usage));

#if CONFIG_BT_HID_HOST_ENABLED
          if (r->transport == ESP_HID_TRANSPORT_BT) {
            printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
            esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
            printf("] srv 0x%03x, ", r->bt.cod.service);
            print_uuid(&r->bt.uuid);
            printf(", ");

            if (is_target_device(r->bda)) {
              ESP_LOGI(TAG, ">>> TARGET DEVICE FOUND! <<<");
              target_result = r;
              device_found = true;
            }
          }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
          printf("NAME: %s ", r->name ? r->name : "");
          printf("\n");
          r = r->next;
        }

#if CONFIG_BT_HID_HOST_ENABLED
        if (device_found && target_result) {
          ESP_LOGI(TAG, "===========================================");
          ESP_LOGI(TAG, "Connecting to target device: " ESP_BD_ADDR_STR,
                   ESP_BD_ADDR_HEX(target_result->bda));
          ESP_LOGI(TAG, "Device name: %s",
                   target_result->name ? target_result->name : "(unknown)");
          ESP_LOGI(TAG, "===========================================");

          memcpy(cached_bda, target_result->bda, sizeof(esp_bd_addr_t));
          cached_transport = target_result->transport;
          has_cached_device = true;

          bt_conn_state = BT_STATE_CONNECTING;
          esp_hidh_dev_open(target_result->bda, target_result->transport, 0);
        }
#endif // CONFIG_BT_HID_HOST_ENABLED

        esp_hid_scan_results_free(results);
        results = NULL;
        results_len = 0;
      }

      if (!device_found) {
        ESP_LOGW(TAG, "Target device not found. Retrying in %d seconds...",
                 SCAN_RETRY_DELAY_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(SCAN_RETRY_DELAY_MS));
      }
      break;
    }
  }
}

// UI update task - handles display and button processing
void ui_task(void *pvParameters) {
  TickType_t last_blink_time = xTaskGetTickCount();
  const TickType_t blink_period = pdMS_TO_TICKS(500);

  while (1) {
    // Toggle blink state every 500ms
    if ((xTaskGetTickCount() - last_blink_time) >= blink_period) {
      blink_state = !blink_state;
      last_blink_time = xTaskGetTickCount();
    }

    // Process any pending button
    button_event_t btn = last_button;
    if (btn != BTN_NONE) {
      last_button = BTN_NONE;
      process_button(btn);
    }

    // Update display based on current state
    switch (scoreboard.state) {
    case GAME_STATE_CONNECTING:
      hub75_show_connecting();
      break;

    case GAME_STATE_DISCONNECTED:
      // Disconnected: 10% brightness, all text blinking
      hub75_set_brightness(10);
      hub75_show_scoreboard(scoreboard.left_score, scoreboard.right_score,
                            scoreboard.left_sets, scoreboard.right_sets,
                            scoreboard.serve_side, blink_state, true);
      break;

    case GAME_STATE_SELECT_FIRST:
      hub75_set_brightness(20);
      hub75_show_select_first(blink_state, -1);
      break;

    case GAME_STATE_PLAYING:
      // Normal play: 20% brightness, always visible (pass true)
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
                           scoreboard.right_sets, blink_state);
      break;

    default:
      hub75_show_connecting();
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz UI update
  }
}

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

  // Initialize scoreboard
  scoreboard_init(&scoreboard);

  // Initialize LED display (20% brightness)
  ESP_ERROR_CHECK(hub75_init());
  hub75_set_brightness(20);
  hub75_start_refresh_task();
  hub75_show_connecting();
  ESP_LOGI(TAG, "LED display initialized (20%% brightness)");

  ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
  ESP_ERROR_CHECK(esp_hid_gap_init(HID_HOST_MODE));

  esp_hidh_config_t config = {
      .callback = hidh_callback,
      .event_stack_size = 4096,
      .callback_arg = NULL,
  };
  ESP_ERROR_CHECK(esp_hidh_init(&config));

  char bda_str[18] = {0};
  ESP_LOGI(TAG, "===========================================");
  ESP_LOGI(TAG, "Digital Table Tennis Scoreboard");
  ESP_LOGI(
      TAG, "Own address: [%s]",
      bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
  ESP_LOGI(TAG, "Target MAC:  [%s]",
           bda2str((uint8_t *)TARGET_MAC, bda_str, sizeof(bda_str)));
  ESP_LOGI(TAG, "===========================================");

  xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL);
  xTaskCreate(&ui_task, "ui_task", 4 * 1024, NULL, 3, NULL);
}
