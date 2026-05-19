#include <utils/vofa.h>

#include "bsp/def.h"
#include "bsp/io.h"
#include "bsp/lcd.h"
#include "bsp/lcd_font.h"
#include "bsp/time.h"
#include "bsp/uart.h"

const bsp_io_t led { GPIO_BOARD_PORT, GPIO_BOARD_LED_PIN }, key { GPIO_BOARD_PORT, GPIO_BOARD_KEY_PIN };

int main() {
    SYSCFG_DL_init();

    bsp_hw_init();
    bsp_uart_init(UART_DEBUG_INST, DMA_UART0_TX_CHAN_ID);

    NVIC_EnableIRQ(GPIO_BOARD_INT_IRQN);

    for (;;) {
        bsp_io_toggle(led);
        bsp_time_delay(50);
    }
}

uint32_t lst_key_ts = 0;

extern "C" void GROUP1_IRQHandler() {
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case DL_INTERRUPT_GROUP1_IIDX_GPIOB:
        if (bsp_io_read(key) == IO_RESET) {
            uint32_t ts = bsp_time_get_ms();
            if (ts - lst_key_ts > 100) {
                bsp_uart_printf_async(UART_DEBUG_INST, "Key pressed %d\r\n", ts);
                lst_key_ts = ts;
            }
        }
        break;
    default:
        break;
    }
}

// 1khz
extern "C" void TIMER_TASK_INST_IRQHandler() {
    switch (DL_TimerG_getPendingInterrupt(TIMER_TASK_INST)) {
    case DL_TIMER_IIDX_ZERO:
        bsp_uart_printf_async(UART_DEBUG_INST, "timer running %d\r\n", bsp_time_get_ms());
        break;
    default:
        break;
    }
}
