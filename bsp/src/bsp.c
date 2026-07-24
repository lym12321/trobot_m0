//
// Created by fish on 2026/5/14.
//

#include "bsp/def.h"
#include "bsp/lcd.h"
#include "bsp/w25q128.h"

const char *volatile bsp_assert_expression;
const char *volatile bsp_assert_message;
const char *volatile bsp_assert_file;
volatile int bsp_assert_line;

#define BSP_CORE_DEBUG_DHCSR (*(volatile uint32_t *)0xE000EDF0u)
#define BSP_CORE_DEBUG_ENABLED (1u << 0)

void bsp_assert_failed(const char *expr, const char *file, int line) {
    bsp_assert_failed_msg(expr, NULL, file, line);
}

void bsp_assert_failed_msg(
    const char *expr, const char *message, const char *file, int line) {
    __disable_irq();
    bsp_assert_expression = expr;
    bsp_assert_message = message;
    bsp_assert_file = file;
    bsp_assert_line = line;

    if ((BSP_CORE_DEBUG_DHCSR & BSP_CORE_DEBUG_ENABLED) != 0u) {
        __BKPT(0);
    }
    for (;;) {
        __WFI();
    }
}

void bsp_hw_init() {
    DL_GPIO_setPins(SPI_CS_PORT, SPI_CS_FLASH_PIN | SPI_CS_LCD_PIN);
    bsp_lcd_init();
    w25q128_init();
}
