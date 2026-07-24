#include "bsp/sys.h"

#include "FreeRTOS.h"
#include "task.h"

bool bsp_sys_in_isr(void) {
    uint32_t result;
    __asm__ volatile("MRS %0, ipsr" : "=r"(result));
    return result != 0u;
}

void bsp_sys_reset(void) {
    NVIC_SystemReset();
}

unsigned long bsp_sys_enter_critical(void) {
    if (bsp_sys_in_isr() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        unsigned long state = (unsigned long)__get_PRIMASK();
        __disable_irq();
        return state;
    }
    taskENTER_CRITICAL();
    return 0;
}

void bsp_sys_exit_critical(unsigned long state) {
    if (bsp_sys_in_isr() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        __set_PRIMASK((uint32_t)state);
    } else {
        taskEXIT_CRITICAL();
    }
}
