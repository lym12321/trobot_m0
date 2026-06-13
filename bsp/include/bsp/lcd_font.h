//
// Created by Codex on 2026/5/19.
//

#pragma once

#include <stdarg.h>
#include <stdio.h>

#include "bsp/lcd.h"

/*
 * Tiny ASCII text renderer for the ST7735 LCD BSP driver.
 *
 * Font:
 * - Built-in 5x7 bitmap font for printable ASCII characters 0x20 .. 0x7E.
 * - One blank column and one blank row are added when drawing, so the logical
 *   character cell is 6x8 pixels at scale 1.
 * - Unsupported characters are rendered as '?'.
 *
 * Scale:
 * - scale must be greater than 0.
 * - scale 1 draws a 6x8 cell.
 * - scale 2 draws a 12x16 cell, and so on.
 *
 * Text layout:
 * - Coordinates use the same top-left origin as lcd.h.
 * - '\n' moves to the next line.
 * - Long text wraps to the next line when it reaches the screen edge.
 * - Drawing stops when the next line would exceed BSP_LCD_HEIGHT.
 *
 * Performance:
 * - scale 1 strings use a line write fast path.
 * - larger scales are drawn character by character.
 *
 * printf:
 * - bsp_lcd_printf() formats into a fixed 128-byte stack buffer before drawing.
 * - The return value is the vsnprintf result, so it may be larger than the
 *   actually displayed buffer length if the formatted text was truncated.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_FONT_WIDTH  5
#define BSP_LCD_FONT_HEIGHT 7

/* 5 columns per glyph, bit 0 is the top pixel, indexed by ch - ' '. */
static const uint8_t bsp_lcd_font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7f, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x04, 0x08, 0x10, 0x08}, // ~
};

/* Draw one printable ASCII character at x/y using RGB565 foreground/background colors. */
static inline void bsp_lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t scale) {
    if (scale == 0) {
        return;
    }

    uint16_t lcd_width = bsp_lcd_get_width();
    uint16_t lcd_height = bsp_lcd_get_height();
    uint16_t char_width = (BSP_LCD_FONT_WIDTH + 1) * scale;
    uint16_t char_height = (BSP_LCD_FONT_HEIGHT + 1) * scale;

    if (x >= lcd_width || y >= lcd_height ||
        x + char_width > lcd_width || y + char_height > lcd_height) {
        return;
    }

    if (ch < ' ' || ch > '~') {
        ch = '?';
    }

    const uint8_t *glyph = bsp_lcd_font5x7[(uint8_t)ch - ' '];

    bsp_lcd_set_address(x, y, x + char_width - 1, y + char_height - 1);
    bsp_lcd_write_begin();

    for (uint8_t row = 0; row < BSP_LCD_FONT_HEIGHT + 1; row++) {
        for (uint8_t sy = 0; sy < scale; sy++) {
            for (uint8_t col = 0; col < BSP_LCD_FONT_WIDTH + 1; col++) {
                uint16_t pixel_color = bg;
                if (row < BSP_LCD_FONT_HEIGHT && col < BSP_LCD_FONT_WIDTH &&
                    (glyph[col] & (uint8_t)(1u << row))) {
                    pixel_color = color;
                }
                bsp_lcd_write_color(pixel_color, scale);
            }
        }
    }

    bsp_lcd_write_end();
}

/* Return the 5-byte glyph for a printable ASCII character, or '?' for unsupported input. */
static inline const uint8_t *bsp_lcd_glyph(char ch) {
    if (ch < ' ' || ch > '~') {
        ch = '?';
    }
    return bsp_lcd_font5x7[(uint8_t)ch - ' '];
}

/* Fast path for one scale-1 text line. Returns the number of characters drawn. */
static inline uint16_t bsp_lcd_print_line_1x(uint16_t x, uint16_t y, const char *text,
                                             uint16_t color, uint16_t bg) {
    uint16_t count = 0;
    uint16_t lcd_width = bsp_lcd_get_width();
    uint16_t lcd_height = bsp_lcd_get_height();
    uint8_t row_buf[BSP_LCD_MAX_WIDTH * 2];

    while (text[count] != '\0' && text[count] != '\n' &&
           x + (count + 1) * (BSP_LCD_FONT_WIDTH + 1) <= lcd_width) {
        count++;
    }
    if (count == 0 || y + BSP_LCD_FONT_HEIGHT + 1 > lcd_height) {
        return 0;
    }

    bsp_lcd_set_address(x, y, x + count * (BSP_LCD_FONT_WIDTH + 1) - 1, y + BSP_LCD_FONT_HEIGHT);
    bsp_lcd_write_begin();

    for (uint8_t row = 0; row < BSP_LCD_FONT_HEIGHT + 1; row++) {
        uint32_t out = 0;

        for (uint16_t i = 0; i < count; i++) {
            const uint8_t *glyph = bsp_lcd_glyph(text[i]);
            for (uint8_t col = 0; col < BSP_LCD_FONT_WIDTH + 1; col++) {
                uint16_t pixel_color = bg;
                if (row < BSP_LCD_FONT_HEIGHT && col < BSP_LCD_FONT_WIDTH &&
                    (glyph[col] & (uint8_t)(1u << row))) {
                    pixel_color = color;
                }
                row_buf[out++] = (uint8_t)(pixel_color >> 8);
                row_buf[out++] = (uint8_t)pixel_color;
            }
        }

        bsp_lcd_write_bytes(row_buf, out);
    }

    bsp_lcd_write_end();
    return count;
}

/* Draw a null-terminated ASCII string with newline and screen-edge wrapping support. */
static inline void bsp_lcd_print(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t scale) {
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    uint16_t lcd_width = bsp_lcd_get_width();
    uint16_t lcd_height = bsp_lcd_get_height();
    uint16_t char_width = (BSP_LCD_FONT_WIDTH + 1) * scale;
    uint16_t char_height = (BSP_LCD_FONT_HEIGHT + 1) * scale;

    if (scale == 0 || text == 0) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            cursor_x = x;
            cursor_y += char_height;
            text++;
            continue;
        }

        if (cursor_x + char_width > lcd_width) {
            cursor_x = x;
            cursor_y += char_height;
        }
        if (cursor_y + char_height > lcd_height) {
            return;
        }

        if (scale == 1) {
            uint16_t count = bsp_lcd_print_line_1x(cursor_x, cursor_y, text, color, bg);
            if (count == 0) {
                return;
            }
            cursor_x += count * char_width;
            text += count;
        } else {
            bsp_lcd_draw_char(cursor_x, cursor_y, *text++, color, bg, scale);
            cursor_x += char_width;
        }
    }
}

/* Format text with printf-style arguments, then draw it with bsp_lcd_print(). */
static inline int bsp_lcd_printf(uint16_t x, uint16_t y, uint16_t color, uint16_t bg, uint8_t scale,
                                 const char *format, ...) {
    char buffer[128];
    va_list args;

    if (format == 0) {
        return -1;
    }

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) {
        return length;
    }

    bsp_lcd_print(x, y, buffer, color, bg, scale);
    return length;
}

#ifdef __cplusplus
}
#endif
