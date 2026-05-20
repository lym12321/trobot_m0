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

extern void example_task(void *args);

void app_entrance(void *args) {
    bsp_hw_init();
    bsp_uart_init(UART0, DMA_UART0_TX_CHAN_ID);

    NVIC_EnableIRQ(GPIO_BOARD_INT_IRQN);

    logger::init(UART_DEBUG_INST, logger::INFO);

    os::task::static_create(example_task, nullptr, "example", 256, os::task::Priority::MEDIUM);

    for (;;) {
        bsp_io_toggle(led);
        os::task::sleep(50);
    }
}

int main() {
    SYSCFG_DL_init();

    xTaskCreate(app_entrance, "entrance", 512, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();

    for (;;) asm("nop");
}