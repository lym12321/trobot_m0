//
// Created by fish & codex on 2026/5/19.
//

#pragma once

#include "bsp/def.h"

/*
 * ST7735 LCD BSP driver.
 *
 * Display geometry:
 * - Logical coordinate origin is the top-left corner.
 * - Valid x range: 0 .. BSP_LCD_WIDTH - 1.
 * - Valid y range: 0 .. BSP_LCD_HEIGHT - 1.
 *
 * Color format:
 * - All public drawing APIs use RGB565.
 * - Use LCD_RGB565(r, g, b) for 8-bit-per-channel RGB input, or the LCD_XXX
 *   color constants below for common colors.
 *
 * SPI sharing:
 * - This LCD shares SPI1 with other devices such as W25Q128.
 * - The driver controls only the LCD chip-select through the BSP SPI device
 *   transaction helpers. Keep each draw operation non-reentrant unless a
 *   higher-level bus lock is added.
 *
 * Pixel stream API:
 * - For custom high-speed drawing, call bsp_lcd_set_address(), then
 *   bsp_lcd_write_begin(), one or more bsp_lcd_write_* calls, and finally
 *   bsp_lcd_write_end().
 */

#define BSP_LCD_WIDTH     128
#define BSP_LCD_HEIGHT    160
#define BSP_LCD_MAX_WIDTH 160

#define LCD_RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xf8u) << 8) | (((uint16_t)(g) & 0xfcu) << 3) | ((uint16_t)(b) >> 3)))

#define LCD_BLACK   0x0000
#define LCD_WHITE   0xffff
#define LCD_RED     0xf800
#define LCD_GREEN   0x07e0
#define LCD_BLUE    0x001f
#define LCD_YELLOW  0xffe0
#define LCD_MAGENTA 0xf81f
#define LCD_CYAN    0x07ff
#define LCD_GRAY    0x8410
#define LCD_ORANGE  0xfd20

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    E_LCD_DIRECTION_0 = 0,
    E_LCD_DIRECTION_90,
    E_LCD_DIRECTION_180,
    E_LCD_DIRECTION_270,
} bsp_lcd_direction_e;

/* Initialize the ST7735 controller and turn on the display/backlight. */
void bsp_lcd_init();

/* Set display scan direction. 90/270 degree modes swap the logical width/height. */
void bsp_lcd_set_direction(bsp_lcd_direction_e direction);

/* Return current logical display size after direction is applied. */
uint16_t bsp_lcd_get_width();
uint16_t bsp_lcd_get_height();

/* Enable or disable the LCD backlight GPIO. */
void bsp_lcd_backlight(uint8_t enable);

/* Send display on/off command without changing GRAM content. */
void bsp_lcd_display(uint8_t enable);

/* Enable or disable ST7735 color inversion mode. */
void bsp_lcd_invert(uint8_t enable);

/* Set the active GRAM write window. Coordinates are clipped to display size. */
void bsp_lcd_set_address(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/* Begin a raw pixel write after bsp_lcd_set_address(). */
void bsp_lcd_write_begin();

/* End a raw pixel write transaction. */
void bsp_lcd_write_end();

/* Write count copies of one RGB565 color into the active window. */
void bsp_lcd_write_color(uint16_t color, uint32_t count);

/* Write raw bytes into the active window. Intended for prepared RGB565 streams. */
void bsp_lcd_write_bytes(const uint8_t *data, uint32_t len);

/* Write count RGB565 pixels into the active window. */
void bsp_lcd_write_rgb565(const uint16_t *data, uint32_t count);

/* Draw one pixel if the coordinate is inside the display. */
void bsp_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/* Fill a clipped rectangle with one RGB565 color. */
void bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/* Fill the whole screen with one RGB565 color. */
void bsp_lcd_clear(uint16_t color);

/* Draw a clipped RGB565 image block. Data is row-major, left-to-right, top-to-bottom. */
void bsp_lcd_draw_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data);

#ifdef __cplusplus
}
#endif
