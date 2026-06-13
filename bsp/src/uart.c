//
// Created by fish on 2026/5/14.
//

#include "bsp/uart.h"
#include "bsp/ds.h"
#include "bsp/sys.h"

#include "stdio.h"
#include "stdarg.h"
#include "string.h"

IRQn_Type irq[BSP_UART_DEVICE_COUNT] = { UART0_INT_IRQn, UART1_INT_IRQn, UART2_INT_IRQn, UART3_INT_IRQn };

ds_rq_t rq[BSP_UART_DEVICE_COUNT];
uint8_t rq_buf[BSP_UART_DEVICE_COUNT][BSP_UART_BUFFER_SIZE];
uint8_t dma_ch[BSP_UART_DEVICE_COUNT];
uint8_t buf[BSP_UART_DEVICE_COUNT][2][BSP_UART_BUFFER_SIZE];
uint8_t busy[BSP_UART_DEVICE_COUNT] = { 0 };

bsp_uart_callback_t callback[BSP_UART_DEVICE_COUNT] = { NULL };

uint8_t idx(bsp_uart_e device) {
    if (device == UART0) return 0;
    if (device == UART1) return 1;
    if (device == UART2) return 2;
    if (device == UART3) return 3;
    return 0;
}

static void rx_fifo_proc(bsp_uart_e device) {
    uint8_t id = idx(device);
    uint32_t len = DL_UART_drainRXFIFO(device, buf[id][1], sizeof(buf[id][1]));
    if (len > 0 && callback[id]) {
        callback[id](device, buf[id][1], len);
    }
}

void bsp_uart_init(bsp_uart_e device, uint8_t tx_dma_ch) {
    uint8_t id = idx(device);
    dma_ch[id] = tx_dma_ch;
    ds_rq_init(&rq[id], rq_buf[id], sizeof(rq_buf[id]));

    DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_setRXInterruptTimeout(device, 8);

    DL_UART_enableInterrupt(device,
        DL_UART_DMA_DONE_INTERRUPT_TX |
        DL_UART_INTERRUPT_RX |
        DL_UART_INTERRUPT_RX_TIMEOUT_ERROR |
        DL_UART_INTERRUPT_OVERRUN_ERROR |
        DL_UART_INTERRUPT_FRAMING_ERROR |
        DL_UART_INTERRUPT_PARITY_ERROR
    );

    NVIC_ClearPendingIRQ(irq[id]);
    NVIC_EnableIRQ(irq[id]);
}

void bsp_uart_send(bsp_uart_e device, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        DL_UART_transmitDataBlocking(device, data[i]);
    }
}

static void send_rq_dma(bsp_uart_e device) {
    // 从 rq 取出一个包发出去
    uint8_t id = idx(device), ch = dma_ch[id];
    uint32_t len = 0; ds_rq_pop(&rq[id], (uint8_t *) &len, sizeof(len));
    ds_rq_pop(&rq[id], buf[id][0], len);

    DL_DMA_setSrcAddr(DMA, ch, (uint32_t) buf[id][0]);
    DL_DMA_setDestAddr(DMA, ch, (uint32_t) &device->TXDATA);
    DL_DMA_setTransferSize(DMA, ch, len);
    DL_DMA_enableChannel(DMA, ch);
}

void bsp_uart_send_async(bsp_uart_e device, const uint8_t *data, uint32_t len) {
    uint8_t id = idx(device);
    if (ds_rq_avail(&rq[id]) < sizeof(len) + len) {
        // 队列爆了发不出去
        return;
    }
    ds_rq_push(&rq[id], (const uint8_t *) &len, sizeof(len));
    ds_rq_push(&rq[id], data, len);

    if (!busy[id]) {
        busy[id] = 1;
        send_rq_dma(device);
    }
}

void bsp_uart_printf(bsp_uart_e device, const char *fmt, ...) {
    char tmp[256] = { 0 };
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len > 0) {
        bsp_uart_send(device, (const uint8_t *)tmp, (uint32_t)len);
    }
}

void bsp_uart_printf_async(bsp_uart_e device, const char *fmt, ...) {
    char tmp[256] = { 0 };
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len > 0) {
        bsp_uart_send_async(device, (const uint8_t *)tmp, (uint32_t)len);
    }
}

void bsp_uart_set_callback(bsp_uart_e device, bsp_uart_callback_t cb) {
    callback[idx(device)] = cb;
}

static uint32_t ulpclk_divisor(void) {
    switch (DL_SYSCTL_getULPCLKDivider()) {
    case DL_SYSCTL_ULPCLK_DIV_1: return 1;
    case DL_SYSCTL_ULPCLK_DIV_2: return 2;
    case DL_SYSCTL_ULPCLK_DIV_3: return 3;
    default: BSP_ASSERT(false); return 1;
    }
}

static uint32_t uart_clock_divisor(DL_UART_CLOCK_DIVIDE_RATIO ratio) {
    switch (ratio) {
    case DL_UART_CLOCK_DIVIDE_RATIO_1: return 1;
    case DL_UART_CLOCK_DIVIDE_RATIO_2: return 2;
    case DL_UART_CLOCK_DIVIDE_RATIO_3: return 3;
    case DL_UART_CLOCK_DIVIDE_RATIO_4: return 4;
    case DL_UART_CLOCK_DIVIDE_RATIO_5: return 5;
    case DL_UART_CLOCK_DIVIDE_RATIO_6: return 6;
    case DL_UART_CLOCK_DIVIDE_RATIO_7: return 7;
    case DL_UART_CLOCK_DIVIDE_RATIO_8: return 8;
    default: BSP_ASSERT(false); return 1;
    }
}

static uint32_t uart_clock_freq(bsp_uart_e device) {
    DL_UART_ClockConfig config;
    DL_UART_getClockConfig(device, &config);

    uint32_t source_freq = 0;
    switch (config.clockSel) {
    case DL_UART_CLOCK_BUSCLK:
        source_freq = CPUCLK_FREQ / ulpclk_divisor();
        break;
    case DL_UART_CLOCK_MFCLK:
        source_freq = 4000000;
        break;
    case DL_UART_CLOCK_LFCLK:
        source_freq = 32768;
        break;
    default:
        BSP_ASSERT(false);
        break;
    }

    return source_freq / uart_clock_divisor(config.divideRatio);
}

void bsp_uart_set_baudrate(bsp_uart_e device, uint32_t baudrate) {
    BSP_ASSERT(baudrate > 0);

    while (DL_UART_isBusy(device)) __asm__ __volatile__ ("nop");

    unsigned long state = bsp_sys_enter_critical();
    DL_UART_changeConfig(device);
    DL_UART_configBaudRate(device, uart_clock_freq(device), baudrate);
    DL_UART_enable(device);
    bsp_sys_exit_critical(state);
}


static void uart_irq_proc(bsp_uart_e device) {
    const uint8_t id = idx(device);
    switch (DL_UART_getPendingInterrupt(device)) {
    case DL_UART_IIDX_DMA_DONE_TX:
        if (ds_rq_size(&rq[id]) == 0) {
            busy[id] = 0;
        } else {
            send_rq_dma(device);
        }
        break;
    case DL_UART_IIDX_RX:
    case DL_UART_IIDX_RX_TIMEOUT_ERROR:
    case DL_UART_IIDX_OVERRUN_ERROR:
    case DL_UART_IIDX_FRAMING_ERROR:
    case DL_UART_IIDX_PARITY_ERROR:
        rx_fifo_proc(device);
        break;
    default: break;
    }
}

void UART0_IRQHandler() {
    uart_irq_proc(UART0);
}

void UART1_IRQHandler() {
    uart_irq_proc(UART1);
}

void UART2_IRQHandler() {
    uart_irq_proc(UART2);
}

void UART3_IRQHandler() {
    uart_irq_proc(UART3);
}
