//
// Created by fish & codex on 2026/5/19.
//

#include "bsp/lcd.h"

#include "bsp/spi.h"
#include "bsp/time.h"

#define LCD_PORT SPI1_INST
#define LCD_MADCTL_MY 0x80
#define LCD_MADCTL_MX 0x40
#define LCD_MADCTL_MV 0x20
#define LCD_TX_BUFFER_SIZE 256u
#define LCD_DMA_MIN_BYTES  32u
#define LCD_DMA_MAX_BYTES  65535u

#ifndef BSP_LCD_USE_DMA
#define BSP_LCD_USE_DMA 1
#endif

#if BSP_LCD_USE_DMA && defined(DMA_SPI1_RX_CHAN_ID) && defined(SPI1_INST_DMA_TRIGGER_0)
#define LCD_USE_RX_DMA 1
#else
#define LCD_USE_RX_DMA 0
#endif

#define dc(x)  ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_DC_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_DC_PIN))
#define res(x) ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_RES_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_RES_PIN))
#define blk(x) ((x) ? DL_GPIO_setPins(GPIO_LCD_PORT, GPIO_LCD_BLK_PIN) : DL_GPIO_clearPins(GPIO_LCD_PORT, GPIO_LCD_BLK_PIN))
#define delay(x) bsp_time_delay(x)

static const bsp_spi_device_t lcd_device = {
    .inst = LCD_PORT,
    .cs_port = SPI_CS_PORT,
    .cs_pin = SPI_CS_LCD_PIN,
};

typedef struct {
    uint8_t madctl;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
} lcd_direction_config_t;

static const lcd_direction_config_t lcd_direction_configs[] = {
    [E_LCD_DIRECTION_0] = {
        .madctl = LCD_MADCTL_MX | LCD_MADCTL_MY,
        .width = BSP_LCD_WIDTH,
        .height = BSP_LCD_HEIGHT,
        .x_offset = 2,
        .y_offset = 1,
    },
    [E_LCD_DIRECTION_90] = {
        .madctl = LCD_MADCTL_MX | LCD_MADCTL_MV,
        .width = BSP_LCD_HEIGHT,
        .height = BSP_LCD_WIDTH,
        .x_offset = 1,
        .y_offset = 2,
    },
    [E_LCD_DIRECTION_180] = {
        .madctl = 0,
        .width = BSP_LCD_WIDTH,
        .height = BSP_LCD_HEIGHT,
        .x_offset = 2,
        .y_offset = 1,
    },
    [E_LCD_DIRECTION_270] = {
        .madctl = LCD_MADCTL_MY | LCD_MADCTL_MV,
        .width = BSP_LCD_HEIGHT,
        .height = BSP_LCD_WIDTH,
        .x_offset = 1,
        .y_offset = 2,
    },
};

static lcd_direction_config_t lcd_config = {
    .madctl = LCD_MADCTL_MX | LCD_MADCTL_MY,
    .width = BSP_LCD_WIDTH,
    .height = BSP_LCD_HEIGHT,
    .x_offset = 2,
    .y_offset = 1,
};

static uint8_t lcd_tx_buffer[LCD_TX_BUFFER_SIZE];
#if LCD_USE_RX_DMA
static volatile uint8_t lcd_rx_dummy;
#endif

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

static void lcd_wait_idle() {
    while (DL_SPI_isBusy(LCD_PORT)) {
    }
}

static void lcd_flush_rx() {
    bsp_spi_flush_rx(LCD_PORT);
}

static void lcd_flush_rx_if_full() {
    if (DL_SPI_isRXFIFOFull(LCD_PORT)) {
        lcd_flush_rx();
    }
}

#if BSP_LCD_USE_DMA
static void lcd_dma_config() {
    DL_DMA_disableChannel(DMA, DMA_SPI1_TX_CHAN_ID);
    DL_DMA_configTransfer(DMA, DMA_SPI1_TX_CHAN_ID,
        DL_DMA_SINGLE_TRANSFER_MODE,
        DL_DMA_NORMAL_MODE,
        DL_DMA_WIDTH_BYTE,
        DL_DMA_WIDTH_BYTE,
        DL_DMA_ADDR_INCREMENT,
        DL_DMA_ADDR_UNCHANGED);
    DL_DMA_setTrigger(DMA, DMA_SPI1_TX_CHAN_ID, SPI1_INST_DMA_TRIGGER_1, DL_DMA_TRIGGER_TYPE_EXTERNAL);

#if LCD_USE_RX_DMA
    DL_DMA_disableChannel(DMA, DMA_SPI1_RX_CHAN_ID);
    DL_DMA_configTransfer(DMA, DMA_SPI1_RX_CHAN_ID,
        DL_DMA_SINGLE_TRANSFER_MODE,
        DL_DMA_NORMAL_MODE,
        DL_DMA_WIDTH_BYTE,
        DL_DMA_WIDTH_BYTE,
        DL_DMA_ADDR_UNCHANGED,
        DL_DMA_ADDR_UNCHANGED);
    DL_DMA_setTrigger(DMA, DMA_SPI1_RX_CHAN_ID, SPI1_INST_DMA_TRIGGER_0, DL_DMA_TRIGGER_TYPE_EXTERNAL);
#endif

    DL_SPI_setFIFOThreshold(LCD_PORT, DL_SPI_RX_FIFO_LEVEL_1_4_FULL, DL_SPI_TX_FIFO_LEVEL_ONE_FRAME);
    DL_SPI_enableDMATransmitEvent(LCD_PORT);
#if LCD_USE_RX_DMA
    DL_SPI_enableDMAReceiveEvent(LCD_PORT, DL_SPI_DMA_INTERRUPT_RX);
#endif
}

static void lcd_write_bytes_dma(const uint8_t *data, uint32_t len) {
    lcd_dma_config();

    while (len > 0) {
        uint16_t chunk = (len > LCD_DMA_MAX_BYTES) ? LCD_DMA_MAX_BYTES : (uint16_t)len;

        lcd_flush_rx();
        DL_DMA_disableChannel(DMA, DMA_SPI1_TX_CHAN_ID);
        DL_DMA_setSrcAddr(DMA, DMA_SPI1_TX_CHAN_ID, (uint32_t)data);
        DL_DMA_setDestAddr(DMA, DMA_SPI1_TX_CHAN_ID, (uint32_t)&LCD_PORT->TXDATA);
        DL_DMA_setTransferSize(DMA, DMA_SPI1_TX_CHAN_ID, chunk);

#if LCD_USE_RX_DMA
        DL_DMA_disableChannel(DMA, DMA_SPI1_RX_CHAN_ID);
        DL_DMA_setSrcAddr(DMA, DMA_SPI1_RX_CHAN_ID, (uint32_t)&LCD_PORT->RXDATA);
        DL_DMA_setDestAddr(DMA, DMA_SPI1_RX_CHAN_ID, (uint32_t)&lcd_rx_dummy);
        DL_DMA_setTransferSize(DMA, DMA_SPI1_RX_CHAN_ID, chunk);
        DL_SPI_clearDMAReceiveEventStatus(LCD_PORT, DL_SPI_DMA_INTERRUPT_RX);
        DL_DMA_enableChannel(DMA, DMA_SPI1_RX_CHAN_ID);
#endif

        DL_SPI_clearDMATransmitEventStatus(LCD_PORT);
        DL_DMA_enableChannel(DMA, DMA_SPI1_TX_CHAN_ID);

#if LCD_USE_RX_DMA
        while (DL_DMA_isChannelEnabled(DMA, DMA_SPI1_TX_CHAN_ID) ||
               DL_DMA_isChannelEnabled(DMA, DMA_SPI1_RX_CHAN_ID)) {
        }
#else
        while (DL_DMA_isChannelEnabled(DMA, DMA_SPI1_TX_CHAN_ID)) {
            if (!DL_SPI_isRXFIFOEmpty(LCD_PORT)) {
                lcd_flush_rx();
            }
        }
#endif
        lcd_wait_idle();
        lcd_flush_rx();

        data += chunk;
        len -= chunk;
    }
}
#endif

static void lcd_write_byte(uint8_t data) {
    while (DL_SPI_isTXFIFOFull(LCD_PORT)) {
        lcd_flush_rx_if_full();
    }

    lcd_flush_rx_if_full();
    DL_SPI_transmitData8(LCD_PORT, data);
}

static void lcd_select() {
    bsp_spi_device_select(&lcd_device);
}

static void lcd_deselect() {
    bsp_spi_device_deselect(&lcd_device);
}

static void lcd_write_command_raw(uint8_t command) {
    dc(0);
    lcd_write_byte(command);
}

static void lcd_write_data_raw(uint8_t data) {
    dc(1);
    lcd_write_byte(data);
}

static void lcd_write_command_data_raw(uint8_t command, const uint8_t *data, uint8_t length) {
    lcd_write_command_raw(command);
    lcd_wait_idle();
    dc(1);
    while (length--) {
        lcd_write_byte(*data++);
    }
}

static void lcd_write_command(uint8_t command) {
    lcd_select();
    lcd_write_command_raw(command);
    lcd_deselect();
}

static void lcd_write_data(uint8_t data) {
    lcd_select();
    lcd_write_data_raw(data);
    lcd_deselect();
}

static void lcd_write_command_data(uint8_t command, const uint8_t *data, uint8_t length) {
    lcd_select();
    lcd_write_command_data_raw(command, data, length);
    lcd_deselect();
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

void bsp_lcd_set_direction(bsp_lcd_direction_e direction) {
    if (direction > E_LCD_DIRECTION_270) {
        return;
    }

    lcd_config = lcd_direction_configs[direction];
    lcd_write_command_data(ST7735_MADCTL, &lcd_config.madctl, 1);
}

uint16_t bsp_lcd_get_width() {
    return lcd_config.width;
}

uint16_t bsp_lcd_get_height() {
    return lcd_config.height;
}

static uint8_t lcd_prepare_address(uint16_t *x0, uint16_t *y0, uint16_t *x1, uint16_t *y1) {
    if (*x0 >= lcd_config.width || *y0 >= lcd_config.height) {
        return 0;
    }
    if (*x1 >= lcd_config.width) {
        *x1 = lcd_config.width - 1;
    }
    if (*y1 >= lcd_config.height) {
        *y1 = lcd_config.height - 1;
    }
    if (*x1 < *x0 || *y1 < *y0) {
        return 0;
    }

    *x0 += lcd_config.x_offset;
    *x1 += lcd_config.x_offset;
    *y0 += lcd_config.y_offset;
    *y1 += lcd_config.y_offset;

    return 1;
}

static void lcd_set_address_raw(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t column[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    uint8_t row[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };

    lcd_write_command_data_raw(ST7735_CASET, column, sizeof(column));
    lcd_write_command_data_raw(ST7735_RASET, row, sizeof(row));
    lcd_write_command_raw(ST7735_RAMWR);
    lcd_wait_idle();
    dc(1);
}

void bsp_lcd_set_address(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (!lcd_prepare_address(&x0, &y0, &x1, &y1)) {
        return;
    }

    bsp_spi_bus_lock();
    lcd_select();
    lcd_set_address_raw(x0, y0, x1, y1);
    lcd_deselect();
    bsp_spi_bus_unlock();
}

void bsp_lcd_write_begin() {
    dc(1);
    lcd_select();
}

void bsp_lcd_write_end() {
    lcd_deselect();
}

static uint8_t lcd_clip_rect(uint16_t *x, uint16_t *y, uint16_t *width, uint16_t *height) {
    if (*x >= lcd_config.width || *y >= lcd_config.height || *width == 0 || *height == 0) {
        return 0;
    }
    if (*x + *width > lcd_config.width) {
        *width = lcd_config.width - *x;
    }
    if (*y + *height > lcd_config.height) {
        *height = lcd_config.height - *y;
    }
    return 1;
}

static void lcd_begin_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    uint16_t x1 = x + width - 1u;
    uint16_t y1 = y + height - 1u;

    (void)lcd_prepare_address(&x, &y, &x1, &y1);
    lcd_select();
    lcd_set_address_raw(x, y, x1, y1);
}

void bsp_lcd_write_color(uint16_t color, uint32_t count) {
    if (count == 0) {
        return;
    }

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)color;
    uint32_t buffered_pixels = count;
    uint32_t max_pixels = sizeof(lcd_tx_buffer) / 2u;

    if (buffered_pixels > max_pixels) {
        buffered_pixels = max_pixels;
    }

    for (uint32_t i = 0; i < buffered_pixels * 2u; i += 2) {
        lcd_tx_buffer[i] = hi;
        lcd_tx_buffer[i + 1u] = lo;
    }

    while (count > 0) {
        uint32_t pixels = count;
        if (pixels > buffered_pixels) {
            pixels = buffered_pixels;
        }

        bsp_lcd_write_bytes(lcd_tx_buffer, pixels * 2u);
        count -= pixels;
    }
}

void bsp_lcd_write_bytes(const uint8_t *data, uint32_t len) {
    if (data == 0) {
        return;
    }

#if BSP_LCD_USE_DMA
    if (len >= LCD_DMA_MIN_BYTES) {
        lcd_write_bytes_dma(data, len);
        return;
    }
#endif

    while (len > 0) {
        while (DL_SPI_isTXFIFOFull(LCD_PORT)) {
            lcd_flush_rx_if_full();
        }

        uint32_t written = DL_SPI_fillTXFIFO8(LCD_PORT, data, len);
        data += written;
        len -= written;
        lcd_flush_rx_if_full();
    }
}

void bsp_lcd_write_rgb565(const uint16_t *data, uint32_t count) {
    if (data == 0) {
        return;
    }

    while (count > 0) {
        uint32_t pixels = count;
        uint32_t max_pixels = sizeof(lcd_tx_buffer) / 2u;
        if (pixels > max_pixels) {
            pixels = max_pixels;
        }

        uint8_t *out = lcd_tx_buffer;
        for (uint32_t i = 0; i < pixels; i++) {
            uint16_t color = data[i];
            *out++ = (uint8_t)(color >> 8);
            *out++ = (uint8_t)color;
        }

        bsp_lcd_write_bytes(lcd_tx_buffer, pixels * 2u);
        data += pixels;
        count -= pixels;
    }
}

void bsp_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    uint16_t width = 1;
    uint16_t height = 1;

    if (!lcd_clip_rect(&x, &y, &width, &height)) {
        return;
    }

    bsp_spi_bus_lock();
    lcd_begin_window(x, y, width, height);
    bsp_lcd_write_color(color, 1);
    lcd_deselect();
    bsp_spi_bus_unlock();
}

void bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!lcd_clip_rect(&x, &y, &width, &height)) {
        return;
    }

    bsp_spi_bus_lock();
    lcd_begin_window(x, y, width, height);
    bsp_lcd_write_color(color, (uint32_t)width * height);
    lcd_deselect();
    bsp_spi_bus_unlock();
}

void bsp_lcd_clear(uint16_t color) {
    bsp_lcd_fill_rect(0, 0, lcd_config.width, lcd_config.height, color);
}

void bsp_lcd_draw_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data) {
    if (data == 0 || !lcd_clip_rect(&x, &y, &width, &height)) {
        return;
    }

    bsp_spi_bus_lock();
    lcd_begin_window(x, y, width, height);
    bsp_lcd_write_rgb565(data, (uint32_t)width * height);
    lcd_deselect();
    bsp_spi_bus_unlock();
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

    bsp_lcd_set_direction(E_LCD_DIRECTION_0);

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
