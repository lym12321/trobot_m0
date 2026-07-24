//
// Created by fish on 2026/5/14.
//

#include "bsp/uart.h"

#include "bsp/sys.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UART_IDLE_TICK_US 50u
#define UART_FRAME_BITS 10u
#define UART_IDLE_FRAMES 2u

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

    uint8_t rx_data[BSP_UART_RX_BUFFER_SIZE];
    uint8_t rx_fifo[16];
    uint16_t rx_length;
    uint16_t idle_ticks;
    uint16_t rx_ticks;
    volatile bool rx_pending;

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

static const uint32_t uart_baudrates[BSP_UART_DEVICE_COUNT] = {
    UART_DEBUG_BAUD_RATE,
    UART1_BAUD_RATE,
    UART2_BAUD_RATE,
    UART3_BAUD_RATE,
};

static const uint32_t uart_frequencies[BSP_UART_DEVICE_COUNT] = {
    UART_DEBUG_INST_FREQUENCY,
    UART1_INST_FREQUENCY,
    UART2_INST_FREQUENCY,
    UART3_INST_FREQUENCY,
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

static uint16_t uart_idle_ticks(uint32_t baudrate) {
    uint32_t idle_us =
        (UART_FRAME_BITS * UART_IDLE_FRAMES * 1000000u + baudrate - 1u) /
        baudrate;
    return (uint16_t)(
        (idle_us + UART_IDLE_TICK_US - 1u) / UART_IDLE_TICK_US + 1u);
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
    state->idle_ticks = uart_idle_ticks(uart_baudrates[id]);
    if (!DL_UART_isFIFOsEnabled(device)) {
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

void bsp_uart_set_callback(bsp_uart_e device, bsp_uart_callback_t callback) {
    const int id = uart_index(device);
    if (id < 0 || !uart_states[id].initialized) {
        return;
    }

    unsigned long irq_state = bsp_sys_enter_critical();
    uart_states[id].callback = callback;
    bsp_sys_exit_critical(irq_state);
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
    DL_UART_configBaudRate(device, uart_frequencies[id], baudrate);
    DL_UART_enableFIFOs(device);
    DL_UART_enable(device);
    state->rx_length = 0u;
    state->rx_ticks = 0u;
    state->rx_pending = false;
    state->idle_ticks = uart_idle_ticks(baudrate);
    DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    bsp_sys_exit_critical(irq_state);
    uart_tx_unlock(state);
    return true;
}

static void uart_drain_rx(int id) {
    bsp_uart_e device = uart_devices[id];
    uart_state_t *state = &uart_states[id];
    uint32_t length;
    bool received = false;
    do {
        length = DL_UART_drainRXFIFO(
            device, state->rx_fifo, sizeof(state->rx_fifo));
        if (length > 0u) {
            received = true;
            uint32_t space = sizeof(state->rx_data) - state->rx_length;
            uint32_t copy_len = length < space ? length : space;
            memcpy(&state->rx_data[state->rx_length], state->rx_fifo, copy_len);
            state->rx_length += (uint16_t)copy_len;
        }
    } while (length > 0u);

    if (received && state->rx_length > 0u) {
        if (!state->rx_pending) {
            DL_UART_setRXFIFOThreshold(device, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
        }
        state->rx_pending = true;
        state->rx_ticks = state->idle_ticks;
        uart_idle_timer_start();
    }
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
            uart_drain_rx(id);
            break;
        case DL_UART_IIDX_OVERRUN_ERROR:
        case DL_UART_IIDX_FRAMING_ERROR:
        case DL_UART_IIDX_PARITY_ERROR:
        case DL_UART_IIDX_BREAK_ERROR:
        case DL_UART_IIDX_NOISE_ERROR:
            uart_drain_rx(id);
            state->rx_length = 0u;
            state->rx_ticks = 0u;
            state->rx_pending = false;
            DL_UART_setRXFIFOThreshold(
                device, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
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

        uart_drain_rx((int)i);
    }

    bool pending = false;
    for (size_t i = 0; i < BSP_UART_DEVICE_COUNT; i++) {
        uart_state_t *state = &uart_states[i];
        if (!state->initialized || !state->rx_pending) {
            continue;
        }

        if (state->rx_ticks > 1u) {
            state->rx_ticks--;
            pending = true;
        } else {
            DL_UART_setRXFIFOThreshold(uart_devices[i], DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
            state->rx_pending = false;
            state->rx_ticks = 0u;
            if (state->callback != NULL && state->rx_length > 0u) {
                state->callback(
                    uart_devices[i], state->rx_data, state->rx_length);
            }
            state->rx_length = 0u;
        }
    }

    if (pending) {
        uart_idle_timer_start();
    }
}
