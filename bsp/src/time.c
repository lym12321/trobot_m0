//
// Created by fish on 2026/5/14.
//

#include "bsp/time.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ti/driverlib/dl_common.h"

static uint32_t tick_to_ms(TickType_t ticks) {
    return (uint32_t)(((uint64_t) ticks * 1000ULL) / configTICK_RATE_HZ);
}

static void busy_delay_ms(uint32_t ms) {
    while (ms-- > 0) {
        DL_Common_delayCycles(configCPU_CLOCK_HZ / 1000U);
    }
}

void bsp_time_delay(uint32_t ms) {
    if (ms == 0) {
        return;
    }

    if ((xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) &&
        (xPortIsInsideInterrupt() == pdFALSE)) {
        TickType_t ticks = pdMS_TO_TICKS(ms);

        if (ticks == 0) {
            ticks = 1;
        }

        vTaskDelay(ticks);
        return;
    }

    busy_delay_ms(ms);
}

uint32_t bsp_time_get_ms() {
    TickType_t ticks;

    if ((xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) &&
        (xPortIsInsideInterrupt() != pdFALSE)) {
        ticks = xTaskGetTickCountFromISR();
    } else {
        ticks = xTaskGetTickCount();
    }

    return tick_to_ms(ticks);
}
