//
// Created by fish on 2026/5/14.
//

#include "bsp/def.h"
#include "bsp/lcd.h"
#include "bsp/w25q128.h"

void bsp_assert_failed(const char *expr, const char *file, int line) {
    // if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    //     __asm volatile("bkpt 0");
    // else {
    //     for (;;) __asm volatile("nop");
    // }
    __asm volatile("bkpt 0");
    for (;;) __asm volatile("nop");
}

void bsp_hw_init() {
    DL_GPIO_setPins(SPI_CS_PORT, SPI_CS_FLASH_PIN | SPI_CS_LCD_PIN);
    bsp_lcd_init();
    w25q128_init();
}
