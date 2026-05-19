//
// Created by fish & codex on 2026/5/19.
//

#include "bsp/lcd.h"

#include "bsp/spi.h"
#include "bsp/time.h"

#define LCD_PORT SPI1_INST
#define LCD_X_OFFSET 2
#define LCD_Y_OFFSET 1

#define dc(x)  ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_DC_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_DC_PIN))
#define res(x) ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_RES_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_RES_PIN))
#define blk(x) ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_BLK_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_BLK_PIN))
#define delay(x) bsp_time_delay(x)

static const bsp_spi_device_t lcd_device = {
    .inst = LCD_PORT,
    .cs_port = SPI_CS_PORT,
    .cs_pin = SPI_CS_LCD_PIN,
};

enum {
    ST7735_SWRESET = 0x01,
    ST7735_SLPOUT  = 0x11,
    ST7735_INVOFF  = 0x20,
    ST7735_INVON   = 0x21,
    ST7735_DISPOFF = 0x28,
    ST7735_DISPON  = 0x29,
    ST7735_CASET   = 0x2A,
    ST7735_RASET   = 0x2B,
    ST7735_RAMWR   = 0x2C,
    ST7735_MADCTL  = 0x36,
    ST7735_COLMOD  = 0x3A,
    ST7735_FRMCTR1 = 0xB1,
    ST7735_FRMCTR2 = 0xB2,
    ST7735_FRMCTR3 = 0xB3,
    ST7735_INVCTR  = 0xB4,
    ST7735_PWCTR1  = 0xC0,
    ST7735_PWCTR2  = 0xC1,
    ST7735_PWCTR3  = 0xC2,
    ST7735_PWCTR4  = 0xC3,
    ST7735_PWCTR5  = 0xC4,
    ST7735_VMCTR1  = 0xC5,
    ST7735_GMCTRP1 = 0xE0,
    ST7735_GMCTRN1 = 0xE1,
};

static void lcd_write_byte(uint8_t data) {
    bsp_spi_transmit(LCD_PORT, data);
}

static void lcd_write_command(uint8_t command) {
    dc(0);
    bsp_spi_device_select(&lcd_device);
    lcd_write_byte(command);
    bsp_spi_device_deselect(&lcd_device);
}

static void lcd_write_data(uint8_t data) {
    dc(1);
    bsp_spi_device_select(&lcd_device);
    lcd_write_byte(data);
    bsp_spi_device_deselect(&lcd_device);
}

static void lcd_write_command_data(uint8_t command, const uint8_t *data, uint8_t length) {
    dc(0);
    bsp_spi_device_select(&lcd_device);
    lcd_write_byte(command);
    dc(1);
    while (length--) {
        lcd_write_byte(*data++);
    }
    bsp_spi_device_deselect(&lcd_device);
}

void bsp_lcd_backlight(uint8_t enable) {
    blk(enable ? 1 : 0);
}

void bsp_lcd_display(uint8_t enable) {
    lcd_write_command(enable ? ST7735_DISPON : ST7735_DISPOFF);
}

void bsp_lcd_invert(uint8_t enable) {
    lcd_write_command(enable ? ST7735_INVON : ST7735_INVOFF);
}

void bsp_lcd_set_address(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (x0 >= BSP_LCD_WIDTH || y0 >= BSP_LCD_HEIGHT) {
        return;
    }
    if (x1 >= BSP_LCD_WIDTH) {
        x1 = BSP_LCD_WIDTH - 1;
    }
    if (y1 >= BSP_LCD_HEIGHT) {
        y1 = BSP_LCD_HEIGHT - 1;
    }
    if (x1 < x0 || y1 < y0) {
        return;
    }

    x0 += LCD_X_OFFSET;
    x1 += LCD_X_OFFSET;
    y0 += LCD_Y_OFFSET;
    y1 += LCD_Y_OFFSET;

    uint8_t column[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    uint8_t row[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };

    lcd_write_command_data(ST7735_CASET, column, sizeof(column));
    lcd_write_command_data(ST7735_RASET, row, sizeof(row));
    lcd_write_command(ST7735_RAMWR);
}

void bsp_lcd_write_begin() {
    dc(1);
    bsp_spi_device_select(&lcd_device);
}

void bsp_lcd_write_end() {
    bsp_spi_device_deselect(&lcd_device);
}

void bsp_lcd_write_color(uint16_t color, uint32_t count) {
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)color;

    while (count--) {
        lcd_write_byte(hi);
        lcd_write_byte(lo);
    }
}

void bsp_lcd_write_rgb565(const uint16_t *data, uint32_t count) {
    if (data == 0) {
        return;
    }

    while (count--) {
        uint16_t color = *data++;
        lcd_write_byte((uint8_t)(color >> 8));
        lcd_write_byte((uint8_t)color);
    }
}

void bsp_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= BSP_LCD_WIDTH || y >= BSP_LCD_HEIGHT) {
        return;
    }

    bsp_lcd_set_address(x, y, x, y);
    bsp_lcd_write_begin();
    bsp_lcd_write_color(color, 1);
    bsp_lcd_write_end();
}

void bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (x >= BSP_LCD_WIDTH || y >= BSP_LCD_HEIGHT || width == 0 || height == 0) {
        return;
    }

    if (x + width > BSP_LCD_WIDTH) {
        width = BSP_LCD_WIDTH - x;
    }
    if (y + height > BSP_LCD_HEIGHT) {
        height = BSP_LCD_HEIGHT - y;
    }

    bsp_lcd_set_address(x, y, x + width - 1, y + height - 1);
    bsp_lcd_write_begin();
    bsp_lcd_write_color(color, (uint32_t)width * height);
    bsp_lcd_write_end();
}

void bsp_lcd_clear(uint16_t color) {
    bsp_lcd_fill_rect(0, 0, BSP_LCD_WIDTH, BSP_LCD_HEIGHT, color);
}

void bsp_lcd_draw_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data) {
    if (data == 0 || x >= BSP_LCD_WIDTH || y >= BSP_LCD_HEIGHT || width == 0 || height == 0) {
        return;
    }

    if (x + width > BSP_LCD_WIDTH) {
        width = BSP_LCD_WIDTH - x;
    }
    if (y + height > BSP_LCD_HEIGHT) {
        height = BSP_LCD_HEIGHT - y;
    }

    bsp_lcd_set_address(x, y, x + width - 1, y + height - 1);
    bsp_lcd_write_begin();
    bsp_lcd_write_rgb565(data, (uint32_t)width * height);
    bsp_lcd_write_end();
}

void bsp_lcd_init() {
    bsp_spi_device_deselect(&lcd_device);
    dc(1);
    blk(0);

    res(0);
    delay(10);
    res(1);
    delay(120);

    lcd_write_command(ST7735_SWRESET);
    delay(150);

    lcd_write_command(ST7735_SLPOUT);
    delay(120);

    const uint8_t frmctr1[] = {0x01, 0x2c, 0x2d};
    lcd_write_command_data(ST7735_FRMCTR1, frmctr1, sizeof(frmctr1));

    const uint8_t frmctr2[] = {0x01, 0x2c, 0x2d};
    lcd_write_command_data(ST7735_FRMCTR2, frmctr2, sizeof(frmctr2));

    const uint8_t frmctr3[] = {0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d};
    lcd_write_command_data(ST7735_FRMCTR3, frmctr3, sizeof(frmctr3));

    lcd_write_command(ST7735_INVCTR);
    lcd_write_data(0x07);

    const uint8_t pwctr1[] = {0xa2, 0x02, 0x84};
    lcd_write_command_data(ST7735_PWCTR1, pwctr1, sizeof(pwctr1));

    lcd_write_command(ST7735_PWCTR2);
    lcd_write_data(0xc5);

    const uint8_t pwctr3[] = {0x0a, 0x00};
    lcd_write_command_data(ST7735_PWCTR3, pwctr3, sizeof(pwctr3));

    const uint8_t pwctr4[] = {0x8a, 0x2a};
    lcd_write_command_data(ST7735_PWCTR4, pwctr4, sizeof(pwctr4));

    const uint8_t pwctr5[] = {0x8a, 0xee};
    lcd_write_command_data(ST7735_PWCTR5, pwctr5, sizeof(pwctr5));

    lcd_write_command(ST7735_VMCTR1);
    lcd_write_data(0x0e);

    lcd_write_command(ST7735_MADCTL);
    lcd_write_data((1 << 7) | (1 << 6));

    const uint8_t gmctrp1[] = {
        0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22,
        0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10,
    };
    lcd_write_command_data(ST7735_GMCTRP1, gmctrp1, sizeof(gmctrp1));

    const uint8_t gmctrn1[] = {
        0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e,
        0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10,
    };
    lcd_write_command_data(ST7735_GMCTRN1, gmctrn1, sizeof(gmctrn1));

    lcd_write_command(0xF0);
    lcd_write_data(0x01);

    lcd_write_command(0xF6);
    lcd_write_data(0x00);

    lcd_write_command(ST7735_COLMOD);
    lcd_write_data(0x05);

    bsp_lcd_clear(LCD_BLACK);
    bsp_lcd_display(1);
    bsp_lcd_backlight(1);
}
