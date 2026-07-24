//
// Created by fish on 2026/5/14.
//

#include "bsp/uart.h"

#include "bsp/sys.h"
#include "bsp/uart_rx.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t length;
    volatile bool ready;
    uint8_t data[BSP_UART_TX_BUFFER_SIZE];
} uart_tx_slot_t;

typedef struct {
    bool initialized;
    uint8_t dma_channel;

    volatile bool tx_busy;
    volatile bool tx_exclusive;
    uint8_t tx_head;
    uint8_t tx_tail;
    volatile uint8_t tx_count;
    uart_tx_slot_t tx_slots[BSP_UART_TX_SLOT_COUNT];

    uint8_t rx_frame[BSP_UART_RX_BUFFER_SIZE];
    uint8_t rx_fifo[16];
    bsp_uart_rx_processor_t rx;
    volatile bool rx_pending;
    volatile bool rx_active;

    bsp_uart_callback_t callback;
} uart_state_t;

static const IRQn_Type uart_irqs[BSP_UART_DEVICE_COUNT] = {
    UART0_INT_IRQn,
    UART1_INT_IRQn,
    UART2_INT_IRQn,
    UART3_INT_IRQn,
};

static bsp_uart_e const uart_devices[BSP_UART_DEVICE_COUNT] = {
    UART0,
    UART1,
    UART2,
    UART3,
};

static const uint8_t uart_dma_channels[BSP_UART_DEVICE_COUNT] = {
#ifdef DMA_UART0_TX_CHAN_ID
    DMA_UART0_TX_CHAN_ID,
#else
    BSP_UART_DMA_CHANNEL_NONE,
#endif
#ifdef DMA_UART1_TX_CHAN_ID
    DMA_UART1_TX_CHAN_ID,
#else
    BSP_UART_DMA_CHANNEL_NONE,
#endif
#ifdef DMA_UART2_TX_CHAN_ID
    DMA_UART2_TX_CHAN_ID,
#else
    BSP_UART_DMA_CHANNEL_NONE,
#endif
#ifdef DMA_UART3_TX_CHAN_ID
    DMA_UART3_TX_CHAN_ID,
#else
    BSP_UART_DMA_CHANNEL_NONE,
#endif
};

static uart_state_t uart_states[BSP_UART_DEVICE_COUNT];
static bool uart_idle_timer_ready;
static volatile bool uart_idle_timer_running;

static int uart_index(bsp_uart_e device) {
    for (int i = 0; i < BSP_UART_DEVICE_COUNT; i++) {
        if (uart_devices[i] == device) {
            return i;
        }
    }
    return -1;
}

static void uart_emit_frame(
    void *context, const uint8_t *data, size_t length) {
    const int id = (int)(uintptr_t)context;
    uart_state_t *state = &uart_states[id];
    if (state->callback != NULL) {
        state->callback(uart_devices[id], data, length);
    }
}

static uint32_t uart_irq_mask(bool dma_enabled) {
    uint32_t mask =
        DL_UART_INTERRUPT_RX |
        DL_UART_INTERRUPT_OVERRUN_ERROR |
        DL_UART_INTERRUPT_FRAMING_ERROR |
        DL_UART_INTERRUPT_PARITY_ERROR |
        DL_UART_INTERRUPT_BREAK_ERROR |
        DL_UART_INTERRUPT_NOISE_ERROR;
    if (dma_enabled) {
        mask |= DL_UART_DMA_DONE_INTERRUPT_TX;
    }
    return mask;
}

static void uart_idle_timer_init(void) {
    if (uart_idle_timer_ready) {
        return;
    }

    DL_TimerG_stopCounter(UART_RX_IDLE_INST);
    DL_TimerG_clearInterruptStatus(UART_RX_IDLE_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(UART_RX_IDLE_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_RX_IDLE_INST_INT_IRQN);
    uart_idle_timer_running = false;
    uart_idle_timer_ready = true;
}

static void uart_idle_timer_start(void) {
    if (uart_idle_timer_running) {
        return;
    }

    DL_TimerG_stopCounter(UART_RX_IDLE_INST);
    DL_TimerG_setTimerCount(UART_RX_IDLE_INST, UART_RX_IDLE_INST_LOAD_VALUE);
    DL_TimerG_clearInterruptStatus(UART_RX_IDLE_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(UART_RX_IDLE_INST_INT_IRQN);
    uart_idle_timer_running = true;
    DL_TimerG_startCounter(UART_RX_IDLE_INST);
}

bool bsp_uart_init(bsp_uart_e device, uint8_t tx_dma_ch) {
    const int id = uart_index(device);
    if (id < 0 ||
        (tx_dma_ch != BSP_UART_DMA_CHANNEL_NONE &&
         (tx_dma_ch >= DMA_SYS_N_DMA_CHANNEL ||
          tx_dma_ch != uart_dma_channels[id]))) {
        return false;
    }

    uart_state_t *state = &uart_states[id];
    unsigned long irq_state = bsp_sys_enter_critical();
    if (state->initialized) {
        bsp_sys_exit_critical(irq_state);
        return false;
    }

    memset(state, 0, sizeof(*state));
    state->dma_channel = tx_dma_ch;
    if (!bsp_uart_rx_processor_init(&state->rx, state->rx_frame, sizeof(state->rx_frame), uart_emit_frame, (void *)(uintptr_t)id)) {
        bsp_sys_exit_critical(irq_state);
        return false;
    }

    const bsp_uart_rx_config_t rx_config = {
        .mode = BSP_UART_RX_MODE_TIMEOUT,
    };
    if (!bsp_uart_rx_processor_configure(&state->rx, &rx_config)) {
        bsp_sys_exit_critical(irq_state);
        return false;
    }
    DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_setRXInterruptTimeout(device, 0);

    const uint32_t irq_mask =
        uart_irq_mask(tx_dma_ch != BSP_UART_DMA_CHANNEL_NONE);
    DL_UART_clearInterruptStatus(device, irq_mask);
    while (DL_UART_drainRXFIFO(
               device, state->rx_fifo, sizeof(state->rx_fifo)) > 0u) {
    }
    DL_UART_enableInterrupt(device, irq_mask);

    state->initialized = true;
    NVIC_ClearPendingIRQ(uart_irqs[id]);
    NVIC_EnableIRQ(uart_irqs[id]);
    uart_idle_timer_init();
    bsp_sys_exit_critical(irq_state);
    return true;
}

static void uart_start_dma(bsp_uart_e device, uart_state_t *state) {
    const uart_tx_slot_t *slot = &state->tx_slots[state->tx_head];
    DL_DMA_setSrcAddr(
        DMA, state->dma_channel, (uint32_t)(uintptr_t)slot->data);
    DL_DMA_setDestAddr(
        DMA, state->dma_channel, (uint32_t)(uintptr_t)&device->TXDATA);
    DL_DMA_setTransferSize(DMA, state->dma_channel, slot->length);
    DL_DMA_enableChannel(DMA, state->dma_channel);
}

static void uart_try_start_dma(bsp_uart_e device, uart_state_t *state) {
    if (!state->tx_busy && state->tx_count > 0u &&
        state->tx_slots[state->tx_head].ready) {
        state->tx_busy = true;
        uart_start_dma(device, state);
    }
}

static bool uart_tx_lock(uart_state_t *state) {
    if (bsp_sys_in_isr() || __get_PRIMASK() != 0u) {
        return false;
    }

    unsigned long irq_state = bsp_sys_enter_critical();
    if (state->tx_exclusive || state->tx_count != 0u) {
        bsp_sys_exit_critical(irq_state);
        return false;
    }
    state->tx_exclusive = true;
    bsp_sys_exit_critical(irq_state);
    return true;
}

static void uart_tx_unlock(uart_state_t *state) {
    unsigned long irq_state = bsp_sys_enter_critical();
    state->tx_exclusive = false;
    bsp_sys_exit_critical(irq_state);
}

bool bsp_uart_send(bsp_uart_e device, const uint8_t *data, uint32_t length) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized ||
        (data == NULL && length != 0u)) {
        return false;
    }
    if (length == 0u) {
        return true;
    }

    uart_state_t *state = &uart_states[id];
    if (!uart_tx_lock(state)) {
        return false;
    }
    for (uint32_t i = 0; i < length; i++) {
        DL_UART_transmitDataBlocking(device, data[i]);
    }
    uart_tx_unlock(state);
    return true;
}

bool bsp_uart_send_async(bsp_uart_e device, const uint8_t *data, uint32_t length) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized ||
        uart_states[id].dma_channel == BSP_UART_DMA_CHANNEL_NONE ||
        (data == NULL && length != 0u) ||
        length > BSP_UART_TX_BUFFER_SIZE) {
        return false;
    }
    if (length == 0u) {
        return true;
    }

    uart_state_t *state = &uart_states[id];
    unsigned long irq_state = bsp_sys_enter_critical();
    if (state->tx_exclusive ||
        state->tx_count == BSP_UART_TX_SLOT_COUNT) {
        bsp_sys_exit_critical(irq_state);
        return false;
    }

    uart_tx_slot_t *slot = &state->tx_slots[state->tx_tail];
    slot->ready = false;
    state->tx_tail =
        (uint8_t)((state->tx_tail + 1u) % BSP_UART_TX_SLOT_COUNT);
    state->tx_count++;
    bsp_sys_exit_critical(irq_state);

    memcpy(slot->data, data, length);
    slot->length = (uint16_t)length;

    irq_state = bsp_sys_enter_critical();
    slot->ready = true;
    uart_try_start_dma(device, state);
    bsp_sys_exit_critical(irq_state);
    return true;
}

static bool uart_vprintf(bsp_uart_e device, const char *fmt, va_list args) {
    if (fmt == NULL) {
        return false;
    }

    char buf[BSP_UART_TX_BUFFER_SIZE];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len <= 0) {
        return len == 0;
    }
    if ((size_t)len >= sizeof(buf)) {
        return false;
    }
    return bsp_uart_send(device, (const uint8_t *)buf, (uint32_t)len);
}

bool bsp_uart_printf(bsp_uart_e device, const char *fmt, ...) {
    if (bsp_sys_in_isr()) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    bool result = uart_vprintf(device, fmt, args);
    va_end(args);
    return result;
}

bool bsp_uart_printf_async(bsp_uart_e device, const char *fmt, ...) {
    if (bsp_sys_in_isr() || fmt == NULL) {
        return false;
    }

    char buf[BSP_UART_TX_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0) {
        return len == 0;
    }
    if ((size_t)len >= sizeof(buf)) {
        return false;
    }
    return bsp_uart_send_async(
        device, (const uint8_t *)buf, (uint32_t)len);
}

bool bsp_uart_configure_rx(bsp_uart_e device, const bsp_uart_rx_config_t *config) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized || config == NULL) {
        return false;
    }

    unsigned long irq_state = bsp_sys_enter_critical();
    bool result =
        bsp_uart_rx_processor_configure(&uart_states[id].rx, config);
    if (result) {
        uart_states[id].rx_pending = false;
        uart_states[id].rx_active = false;
        DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    }
    bsp_sys_exit_critical(irq_state);
    return result;
}

void bsp_uart_set_callback(bsp_uart_e device, bsp_uart_callback_t callback) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized) {
        return;
    }

    unsigned long irq_state = bsp_sys_enter_critical();
    uart_states[id].callback = callback;
    bsp_sys_exit_critical(irq_state);
}

static uint32_t ulpclk_divisor(void) {
    switch (DL_SYSCTL_getULPCLKDivider()) {
    case DL_SYSCTL_ULPCLK_DIV_1:
        return 1;
    case DL_SYSCTL_ULPCLK_DIV_2:
        return 2;
    case DL_SYSCTL_ULPCLK_DIV_3:
        return 3;
    default:
        BSP_ASSERT(false);
        return 1;
    }
}

static uint32_t uart_clock_divisor(DL_UART_CLOCK_DIVIDE_RATIO ratio) {
    switch (ratio) {
    case DL_UART_CLOCK_DIVIDE_RATIO_1:
        return 1;
    case DL_UART_CLOCK_DIVIDE_RATIO_2:
        return 2;
    case DL_UART_CLOCK_DIVIDE_RATIO_3:
        return 3;
    case DL_UART_CLOCK_DIVIDE_RATIO_4:
        return 4;
    case DL_UART_CLOCK_DIVIDE_RATIO_5:
        return 5;
    case DL_UART_CLOCK_DIVIDE_RATIO_6:
        return 6;
    case DL_UART_CLOCK_DIVIDE_RATIO_7:
        return 7;
    case DL_UART_CLOCK_DIVIDE_RATIO_8:
        return 8;
    default:
        BSP_ASSERT(false);
        return 1;
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
        source_freq = 4000000u;
        break;
    case DL_UART_CLOCK_LFCLK:
        source_freq = 32768u;
        break;
    default:
        BSP_ASSERT(false);
        break;
    }

    return source_freq / uart_clock_divisor(config.divideRatio);
}

bool bsp_uart_set_baudrate(bsp_uart_e device, uint32_t baudrate) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized || baudrate == 0u) {
        return false;
    }

    uart_state_t *state = &uart_states[id];
    if (!uart_tx_lock(state)) {
        return false;
    }
    while (DL_UART_isBusy(device)) {
        __asm__ __volatile__("nop");
    }

    unsigned long irq_state = bsp_sys_enter_critical();
    DL_UART_changeConfig(device);
    DL_UART_configBaudRate(device, uart_clock_freq(device), baudrate);
    DL_UART_enable(device);
    bsp_sys_exit_critical(irq_state);
    uart_tx_unlock(state);
    return true;
}

static bool uart_drain_rx(bsp_uart_e device, uart_state_t *state) {
    uint32_t length;
    bool received = false;
    do {
        length = DL_UART_drainRXFIFO(
            device, state->rx_fifo, sizeof(state->rx_fifo));
        if (length > 0u) {
            received = true;
            bsp_uart_rx_processor_feed(
                &state->rx, state->rx_fifo, (size_t)length);
        }
    } while (length == sizeof(state->rx_fifo));

    if (received) {
        if (!state->rx_pending) {
            DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
        }
        state->rx_pending = true;
        state->rx_active = true;
        uart_idle_timer_start();
    }
    return received;
}

static void uart_tx_complete(
    bsp_uart_e device, uart_state_t *state) {
    if (state->tx_count == 0u) {
        state->tx_busy = false;
        return;
    }

    state->tx_head =
        (uint8_t)((state->tx_head + 1u) % BSP_UART_TX_SLOT_COUNT);
    state->tx_count--;
    state->tx_busy = false;
    uart_try_start_dma(device, state);
}

static void uart_irq_proc(bsp_uart_e device) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized) {
        return;
    }

    uart_state_t *state = &uart_states[id];
    for (;;) {
        DL_UART_IIDX interrupt = DL_UART_getPendingInterrupt(device);
        switch (interrupt) {
        case DL_UART_IIDX_DMA_DONE_TX:
            uart_tx_complete(device, state);
            break;
        case DL_UART_IIDX_RX:
            (void)uart_drain_rx(device, state);
            break;
        case DL_UART_IIDX_OVERRUN_ERROR:
        case DL_UART_IIDX_FRAMING_ERROR:
        case DL_UART_IIDX_PARITY_ERROR:
        case DL_UART_IIDX_BREAK_ERROR:
        case DL_UART_IIDX_NOISE_ERROR:
            (void)uart_drain_rx(device, state);
            bsp_uart_rx_processor_error(&state->rx);
            break;
        case DL_UART_IIDX_NO_INTERRUPT:
        default:
            return;
        }
    }
}

void UART0_IRQHandler(void) {
    uart_irq_proc(UART0);
}

void UART1_IRQHandler(void) {
    uart_irq_proc(UART1);
}

void UART2_IRQHandler(void) {
    uart_irq_proc(UART2);
}

void UART3_IRQHandler(void) {
    uart_irq_proc(UART3);
}

void UART_RX_IDLE_INST_IRQHandler(void) {
    if (DL_TimerG_getPendingInterrupt(UART_RX_IDLE_INST) !=
        DL_TIMERG_IIDX_ZERO) {
        return;
    }

    uart_idle_timer_running = false;
    for (size_t i = 0; i < BSP_UART_DEVICE_COUNT; i++) {
        uart_state_t *state = &uart_states[i];
        if (!state->initialized) {
            continue;
        }

        (void)uart_drain_rx(uart_devices[i], state);
    }

    bool pending = false;
    for (size_t i = 0; i < BSP_UART_DEVICE_COUNT; i++) {
        uart_state_t *state = &uart_states[i];
        if (!state->initialized || !state->rx_pending) {
            continue;
        }

        if (state->rx_active) {
            state->rx_active = false;
            pending = true;
        } else {
            DL_UART_setRXFIFOThreshold(uart_devices[i], DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
            state->rx_pending = false;
            bsp_uart_rx_processor_timeout(&state->rx);
        }
    }

    if (pending) {
        uart_idle_timer_start();
    }
}
