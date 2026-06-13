//
// Created by fish on 2026/5/21.
//

#include "bsp/lcd.h"
#include "bsp/lcd_font.h"

#include "bsp/time.h"

#include "utils/logger.h"
#include "utils/os.h"

void example_task(void *args) {
    bsp_lcd_set_direction(E_LCD_DIRECTION_180);
    bsp_lcd_backlight(1);
    // bsp_lcd_clear(LCD_RGB565(126, 12, 110));
    bsp_lcd_clear(LCD_BLACK);

    bsp_lcd_printf(0, 0, LCD_WHITE, LCD_BLACK, 2, "trobot_m0");
    bsp_lcd_printf(0, 100, LCD_WHITE, LCD_BLACK, 1, "man what can i say???");

    for (;;) {
        logger::info("Hello, world! Time: %d ms", bsp_time_get_ms());
        bsp_lcd_printf(0, 30, LCD_WHITE, LCD_BLACK, 1, "rtos time: %d", bsp_time_get_ms());
        os::task::sleep(1);
    }
}
