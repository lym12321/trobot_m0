//
// Created by fish on 2026/5/14.
//

#include "bsp/time.h"

#include "ti_msp_dl_config.h"

volatile uint32_t delay_ms = 0, sys_ts = 0;

void SysTick_Handler(void) {
    sys_ts ++;
    if (delay_ms > 0) {
        delay_ms--;
    }
}

void bsp_time_delay(uint32_t ms) {
    delay_ms = ms;
    while (delay_ms > 0) asm("nop");
}

uint32_t bsp_time_get_ms() {
    return sys_ts;
}
