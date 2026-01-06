/*
 * Large Font for LED Display (11x14 pixels)
 * Numbers 0-9 and colon for score display
 * 3-pixel thick strokes, 11 columns wide
 * Each column uses 14 bits (bits 0-13), LSB at top
 */

#ifndef FONT_LARGE_H
#define FONT_LARGE_H

#include <stdint.h>
#include <stddef.h>

#define FONT_LARGE_WIDTH 11
#define FONT_LARGE_HEIGHT 14

// Large font data - each digit is 11 columns x 14 rows
// 3-pixel thick strokes
// Stored as 11 x uint16_t per character (14 bits used per column, LSB = top)
static const uint16_t font_large_digits[][FONT_LARGE_WIDTH] = {
    // 0 - rounded rectangle
    {
        0x07F0, 0x1FFC, 0x3FFE, 0x3C1E, 0x300E, 
        0x3006, 0x300E, 0x3C1E, 0x3FFE, 0x1FFC, 0x07F0
    },
    // 1 - vertical line with base
    {
        0x0000, 0x0000, 0x3018, 0x301C, 0x301E, 
        0x3FFE, 0x3FFE, 0x3FFE, 0x3000, 0x0000, 0x0000
    },
    // 2 - 
    {
        0x3018, 0x3C1C, 0x3E0E, 0x3F06, 0x3786, 
        0x33C6, 0x31E6, 0x30FE, 0x307E, 0x303C, 0x3018
    },
    // 3 - 
    {
        0x0C0C, 0x1C0E, 0x3C1E, 0x3186, 0x3186, 
        0x3186, 0x3186, 0x3BCE, 0x3FFE, 0x1E7C, 0x0C38
    },
    // 4 - 
    {
        0x0780, 0x07C0, 0x07E0, 0x0670, 0x0638, 
        0x061C, 0x3FFE, 0x3FFE, 0x3FFE, 0x0600, 0x0600
    },
    // 5 - 
    {
        0x0C7E, 0x1C7E, 0x3C7E, 0x3066, 0x3066, 
        0x3066, 0x3066, 0x38E6, 0x3FE6, 0x1FC6, 0x0F86
    },
    // 6 - 
    {
        0x07F0, 0x1FF8, 0x3FFC, 0x398C, 0x3186, 
        0x3186, 0x3186, 0x398E, 0x3F9C, 0x1F18, 0x0E00
    },
    // 7 - 
    {
        0x0006, 0x0006, 0x0006, 0x3E06, 0x3F86, 
        0x3FC6, 0x01E6, 0x00F6, 0x007E, 0x003E, 0x001E
    },
    // 8 - 
    {
        0x0E38, 0x1F7C, 0x3FFE, 0x39CE, 0x3186, 
        0x3186, 0x39CE, 0x3FFE, 0x3FFE, 0x1F7C, 0x0E38
    },
    // 9 - 
    {
        0x0038, 0x0C7C, 0x1CFE, 0x38CE, 0x30C6, 
        0x30C6, 0x38CE, 0x1FFE, 0x1FFC, 0x0FF8, 0x07F0
    }
};

// Colon character (4 pixels wide)
#define FONT_LARGE_COLON_WIDTH 4
static const uint16_t font_large_colon[FONT_LARGE_COLON_WIDTH] = {
    0x0000, 0x0E38, 0x0E38, 0x0000
};

/**
 * Get font data for a large digit
 * @param digit Character '0'-'9'
 * @return Pointer to 11 uint16_t of font data, or NULL if invalid
 */
static inline const uint16_t *font_large_get_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return font_large_digits[digit - '0'];
    }
    return NULL;
}

/**
 * Get colon font data
 * @return Pointer to 4 uint16_t of font data
 */
static inline const uint16_t *font_large_get_colon(void) {
    return font_large_colon;
}

#endif // FONT_LARGE_H
