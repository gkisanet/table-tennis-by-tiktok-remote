/*
 * HUB75 LED Display Driver for P5 64x32 Panel
 * Simplified I2S DMA-based driver for ESP32
 */

#ifndef HUB75_DISPLAY_H
#define HUB75_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Panel configuration
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

// HUB75 GPIO Pin mapping (user-defined)
#define PIN_R1 25
#define PIN_G1 26
#define PIN_B1 27
#define PIN_R2 14
#define PIN_G2 12
#define PIN_B2 13

#define PIN_A 23
#define PIN_B 22
#define PIN_C 5
#define PIN_D 17

#define PIN_CLK 16
#define PIN_LAT 4
#define PIN_OE 15

// Color definitions (RGB565 format)
// Orange (R+G) for main scores - better visibility and power efficiency
#define COLOR_BLACK 0x0000
#define COLOR_RED 0xFFE0          // 주황색 (R+G) - 메인 점수용
#define COLOR_PURE_RED 0xF800     // 순수 빨간색 - 연결 끊김용
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_WHITE 0xFFFF
#define COLOR_RED_DARK 0x3320     // 어두운 주황색 (깜빡임/연결끊김용)
#define COLOR_GREEN_DARK 0x0300   // 어두운 초록

/**
 * @brief Initialize the HUB75 display
 * @return ESP_OK on success
 */
esp_err_t hub75_init(void);

/**
 * @brief Deinitialize the HUB75 display
 */
void hub75_deinit(void);

/**
 * @brief Set display brightness (0-100%)
 * @param brightness Brightness percentage (0-100)
 */
void hub75_set_brightness(uint8_t brightness);

/**
 * @brief Clear the display buffer
 */
void hub75_clear(void);

/**
 * @brief Set a pixel color
 * @param x X coordinate (0 to PANEL_WIDTH-1)
 * @param y Y coordinate (0 to PANEL_HEIGHT-1)
 * @param color RGB565 color value
 */
void hub75_set_pixel(int x, int y, uint16_t color);

/**
 * @brief Display text on the panel (5x7 font)
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param text Text to display
 * @param color Text color (RGB565)
 */
void hub75_draw_text(int x, int y, const char *text, uint16_t color);

/**
 * @brief Draw a large digit (10x14 font)
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param digit Digit character '0'-'9'
 * @param color Text color (RGB565)
 */
void hub75_draw_large_digit(int x, int y, char digit, uint16_t color);

/**
 * @brief Draw a large colon (4x14)
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param color Text color (RGB565)
 */
void hub75_draw_large_colon(int x, int y, uint16_t color);

/**
 * @brief Draw a tally bar (serve indicator)
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param width Bar width in pixels
 * @param active true = visible, false = hidden
 * @param color Bar color (RGB565)
 */
void hub75_draw_tally(int x, int y, int width, bool active, uint16_t color);

/**
 * @brief Update the display (refresh from buffer)
 */
void hub75_update(void);

/**
 * @brief Start the display refresh task
 */
void hub75_start_refresh_task(void);

// ============================================
// Scoreboard UI Functions
// ============================================

/**
 * @brief Display "Connecting..." message
 */
void hub75_show_connecting(void);

/**
 * @brief Display the main scoreboard
 * @param left_score Left player score (0-99)
 * @param right_score Right player score (0-99)
 * @param left_sets Left player sets won
 * @param right_sets Right player sets won
 * @param serve_side Current server (0=left, 1=right, -1=none)
 * @param blink_on Blink state (true = visible, for disconnected blinking)
 * @param disconnected true = disconnected state (pure red color)
 */
void hub75_show_scoreboard(int left_score, int right_score, 
                           int left_sets, int right_sets,
                           int serve_side, bool blink_on, bool disconnected);

/**
 * @brief Display "Select First" screen with blinking tally bars
 * @param blink_on Blink state (true = visible)
 * @param selected_side Selected side (-1=none/both blink, 0=left, 1=right)
 */
void hub75_show_select_first(bool blink_on, int selected_side);

/**
 * @brief Display winner announcement with blinking tally
 * @param winner_side Winner side (0=left, 1=right)
 * @param blink_on Blink state (true = visible)
 * @param left_sets Left player sets
 * @param right_sets Right player sets
 * @param left_score Final left score
 * @param right_score Final right score
 */
void hub75_show_winner(int winner_side, bool blink_on, int left_sets, int right_sets,
                       int left_score, int right_score);

/**
 * @brief Display "Reset?" confirmation dialog
 */
void hub75_show_confirm_reset(void);

/**
 * @brief Display menu with Switch/Stop options
 * @param selection Current selection (0=Switch, 1=Stop)
 */
void hub75_show_menu(int selection);

/**
 * @brief Display match end screen
 * @param winner_side Match winner (0=left, 1=right)
 * @param left_sets Left player final sets
 * @param right_sets Right player final sets
 * @param blink_on Blink state
 */
void hub75_show_match_end(int winner_side, int left_sets, int right_sets, bool blink_on);

#endif // HUB75_DISPLAY_H
