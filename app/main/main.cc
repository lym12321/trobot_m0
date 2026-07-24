#include "FreeRTOS.h"
#include "task.h"

#include "bsp/def.h"
#include "bsp/io.h"
#include "bsp/time.h"
#include "bsp/uart.h"

#include "utils/logger.h"
#include "utils/os.h"

const bsp_io_t led { GPIO_BOARD_PORT, GPIO_BOARD_LED_PIN }, key { GPIO_BOARD_PORT, GPIO_BOARD_KEY_PIN };

uint32_t lst_key_ts = 0;
volatile bool key_log_pending = false;
volatile uint32_t key_log_ts = 0;

extern "C" void GROUP1_IRQHandler() {
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case DL_INTERRUPT_GROUP1_IIDX_GPIOB:
        if (bsp_io_read(key) == IO_RESET) {
            uint32_t ts = bsp_time_get_ms();
            if (ts - lst_key_ts > 100) {
                lst_key_ts = ts;
                key_log_ts = ts;
                key_log_pending = true;
            }
        }
        break;
    default:
        break;
    }
}

extern void example_task(void *args);

void app_entrance(void *args) {
    bsp_hw_init();
    bsp_uart_init(UART0, DMA_UART0_TX_CHAN_ID);

    NVIC_EnableIRQ(GPIO_BOARD_INT_IRQN);

    logger::init(UART_DEBUG_INST, logger::INFO);

    os::task::static_create(example_task, nullptr, "example", 256, os::task::Priority::MEDIUM);

    for (;;) {
        unsigned long irq_state = bsp_sys_enter_critical();
        bool log_key = key_log_pending;
        uint32_t key_ts = key_log_ts;
        key_log_pending = false;
        bsp_sys_exit_critical(irq_state);
        if (log_key) {
            (void)bsp_uart_printf_async(UART_DEBUG_INST, "Key pressed %d\r\n", key_ts);
        }

        bsp_io_toggle(led);
        os::task::sleep(50);
    }
}

int main() {
    SYSCFG_DL_init();
    // FreeRTOS owns SysTick. SysConfig currently starts it before the kernel
    // data structures are ready, so stop it until vTaskStartScheduler()
    // configures and enables the tick.
    DL_SYSTICK_disable();

    xTaskCreate(app_entrance, "entrance", 512, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();

    for (;;) asm("nop");
}
