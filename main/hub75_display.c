/*
 * HUB75 LED Display Driver for P5 64x32 Panel
 * Simplified GPIO bit-banging driver for ESP32
 *
 * Uses software rendering with FreeRTOS task for refresh
 */

#include "hub75_display.h"
#include "font5x7.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "HUB75";

// Frame buffer - RGB values for each pixel
// We use 1 bit per color for simplicity (on/off per color channel)
static uint8_t frame_buffer[PANEL_HEIGHT][PANEL_WIDTH][3]; // [y][x][RGB]

// Brightness (0-100)
static uint8_t brightness = 20;

// Refresh task handle
static TaskHandle_t refresh_task_handle = NULL;
static volatile bool refresh_running = false;

// Initialize a single GPIO pin as output
static void init_gpio_output(int pin) {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  gpio_set_level(pin, 0);
}

// Set row address (A, B, C, D for 1/16 scan)
static inline void set_row_address(int row) {
  gpio_set_level(PIN_A, (row >> 0) & 1);
  gpio_set_level(PIN_B, (row >> 1) & 1);
  gpio_set_level(PIN_C, (row >> 2) & 1);
  gpio_set_level(PIN_D, (row >> 3) & 1);
}

// Clock pulse
static inline void clock_pulse(void) {
  gpio_set_level(PIN_CLK, 1);
  gpio_set_level(PIN_CLK, 0);
}

// Latch data
static inline void latch_data(void) {
  gpio_set_level(PIN_LAT, 1);
  gpio_set_level(PIN_LAT, 0);
}

// Enable/disable output (OE is active low)
static inline void set_output_enable(bool enable) {
  gpio_set_level(PIN_OE, enable ? 0 : 1);
}

// Refresh one frame
static void refresh_frame(void) {
  // 64x32 panel with 1/16 scan: 16 row groups, 2 rows lit at once
  for (int row = 0; row < 16; row++) {
    // 1. Disable output first (prevents ghosting during row switch)
    set_output_enable(false);

    // 2. Shift out data for this row while display is off
    for (int col = 0; col < PANEL_WIDTH; col++) {
      // Upper half (row 0-15)
      int y1 = row;
      // Lower half (row 16-31)
      int y2 = row + 16;

      // Set R1, G1, B1 (upper half)
      gpio_set_level(PIN_R1, frame_buffer[y1][col][0] > 0 ? 1 : 0);
      gpio_set_level(PIN_G1, frame_buffer[y1][col][1] > 0 ? 1 : 0);
      gpio_set_level(PIN_B1, frame_buffer[y1][col][2] > 0 ? 1 : 0);

      // Set R2, G2, B2 (lower half)
      gpio_set_level(PIN_R2, frame_buffer[y2][col][0] > 0 ? 1 : 0);
      gpio_set_level(PIN_G2, frame_buffer[y2][col][1] > 0 ? 1 : 0);
      gpio_set_level(PIN_B2, frame_buffer[y2][col][2] > 0 ? 1 : 0);

      // Clock pulse
      clock_pulse();
    }

    // CRITICAL SECTION: Prevent interrupts during row switch timing
    // This ensures consistent row display time
    portDISABLE_INTERRUPTS();

    // 3. Set row address BEFORE latching
    set_row_address(row);

    // 4. Latch the data to output registers
    latch_data();

    // 5. Enable output for brightness-controlled duration
    set_output_enable(true);
    esp_rom_delay_us(brightness);

    // 6. Disable output before next row
    set_output_enable(false);

    portENABLE_INTERRUPTS();
  }
}

// Refresh task - runs at moderate priority with proper delays
static void refresh_task(void *arg) {
  ESP_LOGI(TAG, "Refresh task started");

  while (refresh_running) {
    refresh_frame();
    // vTaskDelay(1) = minimum 1 tick delay to allow IDLE task to run (watchdog)
    vTaskDelay(1);
  }

  ESP_LOGI(TAG, "Refresh task stopped");
  vTaskDelete(NULL);
}

esp_err_t hub75_init(void) {
  ESP_LOGI(TAG, "Initializing HUB75 display (64x32, 1/16 scan)");

  // Initialize all GPIO pins
  init_gpio_output(PIN_R1);
  init_gpio_output(PIN_G1);
  init_gpio_output(PIN_B1);
  init_gpio_output(PIN_R2);
  init_gpio_output(PIN_G2);
  init_gpio_output(PIN_B2);

  init_gpio_output(PIN_A);
  init_gpio_output(PIN_B);
  init_gpio_output(PIN_C);
  init_gpio_output(PIN_D);

  init_gpio_output(PIN_CLK);
  init_gpio_output(PIN_LAT);
  init_gpio_output(PIN_OE);

  // Disable output initially
  set_output_enable(false);

  // Clear the frame buffer
  hub75_clear();

  ESP_LOGI(TAG, "HUB75 GPIO initialized");

  return ESP_OK;
}

void hub75_deinit(void) {
  refresh_running = false;
  if (refresh_task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for task to exit
    refresh_task_handle = NULL;
  }
  set_output_enable(false);
}

void hub75_set_brightness(uint8_t b) {
  if (b > 100)
    b = 100;
  if (b != brightness) {
    brightness = b;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
  }
}

void hub75_clear(void) { memset(frame_buffer, 0, sizeof(frame_buffer)); }

void hub75_set_pixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
    return;
  }

  // Convert RGB565 to RGB (1 bit per channel for now)
  frame_buffer[y][x][0] = (color & 0xF800) ? 255 : 0; // Red
  frame_buffer[y][x][1] = (color & 0x07E0) ? 255 : 0; // Green
  frame_buffer[y][x][2] = (color & 0x001F) ? 255 : 0; // Blue
}

void hub75_draw_text(int x, int y, const char *text, uint16_t color) {
  if (text == NULL)
    return;

  int cursor_x = x;

  while (*text) {
    const uint8_t *char_data = font5x7_get_char(*text);

    // Draw character (5 columns, 7 rows)
    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
      uint8_t column_bits = char_data[col];

      for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        if (column_bits & (1 << row)) {
          hub75_set_pixel(cursor_x + col, y + row, color);
        }
      }
    }

    // Move cursor (5 pixels + 1 space)
    cursor_x += FONT_CHAR_WIDTH + 1;
    text++;

    // Stop if we're off the screen
    if (cursor_x >= PANEL_WIDTH)
      break;
  }
}

void hub75_update(void) {
  // Single frame refresh
  refresh_frame();
}

void hub75_start_refresh_task(void) {
  if (refresh_task_handle != NULL) {
    ESP_LOGW(TAG, "Refresh task already running");
    return;
  }

  refresh_running = true;
  // Priority 5 is good balance between display stability and system health
  xTaskCreate(refresh_task, "hub75_refresh", 2048, NULL, 5,
              &refresh_task_handle);
}

// ============================================
// Large Font Drawing Functions
// ============================================

#include "font_large.h"

void hub75_draw_large_digit(int x, int y, char digit, uint16_t color) {
  const uint16_t *font_data = font_large_get_digit(digit);
  if (font_data == NULL)
    return;

  for (int col = 0; col < FONT_LARGE_WIDTH; col++) {
    uint16_t column_bits = font_data[col];
    for (int row = 0; row < FONT_LARGE_HEIGHT; row++) {
      if (column_bits & (1 << row)) {
        hub75_set_pixel(x + col, y + row, color);
      }
    }
  }
}

void hub75_draw_large_colon(int x, int y, uint16_t color) {
  const uint16_t *font_data = font_large_get_colon();

  for (int col = 0; col < FONT_LARGE_COLON_WIDTH; col++) {
    uint16_t column_bits = font_data[col];
    for (int row = 0; row < FONT_LARGE_HEIGHT; row++) {
      if (column_bits & (1 << row)) {
        hub75_set_pixel(x + col, y + row, color);
      }
    }
  }
}

void hub75_draw_tally(int x, int y, int width, bool active, uint16_t color) {
  if (!active)
    return;

  // Draw a horizontal bar (2 pixels tall)
  for (int px = 0; px < width; px++) {
    hub75_set_pixel(x + px, y, color);
    hub75_set_pixel(x + px, y + 1, color);
  }
}

// ============================================
// Scoreboard UI Functions
// ============================================

// Draw two-digit score at position
static void draw_score(int x, int y, int score, uint16_t color) {
  char tens = '0' + (score / 10) % 10;
  char ones = '0' + (score % 10);

  hub75_draw_large_digit(x, y, tens, color);
  hub75_draw_large_digit(x + FONT_LARGE_WIDTH + 1, y, ones, color);
}

void hub75_show_connecting(void) {
  hub75_clear();
  // "Connecting..." centered
  hub75_draw_text(2, 12, "Connecting..", COLOR_RED);
}

void hub75_show_scoreboard(int left_score, int right_score, int left_sets,
                           int right_sets, int serve_side, bool blink_on,
                           bool disconnected) {
  hub75_clear();

  // Use pure red when disconnected, orange otherwise
  uint16_t main_color = disconnected ? COLOR_PURE_RED : COLOR_RED;
  uint16_t tally_color = COLOR_GREEN;

  // Set scores display (top center) - small font
  char set_str[8];
  set_str[0] = '0' + left_sets;
  set_str[1] = ' ';
  set_str[2] = ':';
  set_str[3] = ' ';
  set_str[4] = '0' + right_sets;
  set_str[5] = '\0';

  // Draw all elements (blink_on controls visibility in disconnected state)
  if (blink_on) {
    hub75_draw_text(18, 0, set_str, main_color);
    hub75_draw_tally(2, 8, 23, serve_side == 0, tally_color);
    hub75_draw_tally(39, 8, 23, serve_side == 1, tally_color);
    // Main scores (Y=10, moved 1px up from 11)
    draw_score(2, 10, left_score, main_color);
    hub75_draw_large_colon(30, 10, main_color);
    draw_score(39, 10, right_score, main_color);

    // Show "Disconn" at bottom when disconnected
    if (disconnected) {
      hub75_draw_text(14, 25, "Disconn", COLOR_PURE_RED);
    }
  }
}

void hub75_show_select_first(bool blink_on, int selected_side) {
  hub75_clear();

  // Set scores (0:0 at top)
  hub75_draw_text(18, 0, "0 : 0", COLOR_RED);

  // Both tally bars blink if no side selected (-1)
  uint16_t left_tally_color = COLOR_BLACK;
  uint16_t right_tally_color = COLOR_BLACK;

  if (selected_side == -1) {
    left_tally_color = blink_on ? COLOR_GREEN : COLOR_BLACK;
    right_tally_color = blink_on ? COLOR_GREEN : COLOR_BLACK;
  } else if (selected_side == 0) {
    left_tally_color = COLOR_GREEN;
    right_tally_color = COLOR_BLACK;
  } else {
    left_tally_color = COLOR_BLACK;
    right_tally_color = COLOR_GREEN;
  }

  // Tally bars - moved 2px toward center
  hub75_draw_tally(2, 8, 23, true, left_tally_color);
  hub75_draw_tally(39, 8, 23, true, right_tally_color);

  // Scores "00 : 00" - Y=10, moved 1px up
  draw_score(2, 10, 0, COLOR_RED);
  hub75_draw_large_colon(30, 10, COLOR_RED);
  draw_score(39, 10, 0, COLOR_RED);
}

void hub75_show_winner(int winner_side, bool blink_on, int left_sets,
                       int right_sets, int left_score, int right_score) {
  hub75_clear();

  // Set scores (top center)
  char set_str[8];
  set_str[0] = '0' + left_sets;
  set_str[1] = ' ';
  set_str[2] = ':';
  set_str[3] = ' ';
  set_str[4] = '0' + right_sets;
  set_str[5] = '\0';
  hub75_draw_text(18, 0, set_str, COLOR_RED);

  // Blinking tally on winner side only
  uint16_t tally_color = blink_on ? COLOR_GREEN : COLOR_BLACK;
  hub75_draw_tally(2, 8, 23, winner_side == 0, tally_color);
  hub75_draw_tally(39, 8, 23, winner_side == 1, tally_color);

  // Show final game score (Y=10, same as playing screen)
  draw_score(2, 10, left_score, COLOR_RED);
  hub75_draw_large_colon(30, 10, COLOR_RED);
  draw_score(39, 10, right_score, COLOR_RED);

  // "OK" hint at bottom center (blinking)
  if (blink_on) {
    hub75_draw_text(26, 25, "OK", COLOR_GREEN);
  }
}

void hub75_show_confirm_reset(void) {
  hub75_clear();

  // "Reset?" in large-ish text (using normal font, centered)
  hub75_draw_text(14, 8, "Reset?", COLOR_RED);

  // "OK" button hint at bottom
  hub75_draw_text(26, 22, "OK", COLOR_GREEN);
}

void hub75_show_menu(int selection) {
  hub75_clear();

  // Vertical menu layout
  // Switch on top, Stop on bottom

  // Row 1: "Switch   [0]" or "Switch   [ ]"
  hub75_draw_text(2, 4, "Switch", COLOR_RED);
  if (selection == 0) {
    hub75_draw_text(42, 4, "[0]", COLOR_GREEN);
  } else {
    hub75_draw_text(42, 4, "[ ]", COLOR_RED_DARK);
  }

  // Row 2: "Stop     [0]" or "Stop     [ ]"
  hub75_draw_text(2, 16, "Stop", COLOR_RED);
  if (selection == 1) {
    hub75_draw_text(42, 16, "[0]", COLOR_GREEN);
  } else {
    hub75_draw_text(42, 16, "[ ]", COLOR_RED_DARK);
  }
}

void hub75_show_match_end(int winner_side, int left_sets, int right_sets,
                          bool blink_on) {
  hub75_clear();

  // Final set scores
  char set_str[8];
  set_str[0] = '0' + left_sets;
  set_str[1] = ' ';
  set_str[2] = ':';
  set_str[3] = ' ';
  set_str[4] = '0' + right_sets;
  set_str[5] = '\0';
  hub75_draw_text(18, 0, set_str, COLOR_RED);

  // "WIN!" message on winner side (blinking)
  if (blink_on) {
    if (winner_side == 0) {
      hub75_draw_text(4, 14, "WIN!", COLOR_GREEN);
    } else {
      hub75_draw_text(40, 14, "WIN!", COLOR_GREEN);
    }
  }
}
