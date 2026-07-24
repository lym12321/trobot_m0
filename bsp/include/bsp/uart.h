//
// Created by fish on 2026/5/14.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/def.h"
#include "bsp/uart_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_UART_DEVICE_COUNT 4u
#define BSP_UART_RX_BUFFER_SIZE 128u
#define BSP_UART_TX_BUFFER_SIZE 256u
#define BSP_UART_TX_SLOT_COUNT 2u
#define BSP_UART_DMA_CHANNEL_NONE UINT8_MAX

/* Kept for source compatibility. New code should use the RX/TX-specific sizes. */
#define BSP_UART_BUFFER_SIZE BSP_UART_RX_BUFFER_SIZE

typedef bsp_uart_handle_t bsp_uart_e;
typedef void (*bsp_uart_callback_t)(
    bsp_uart_e device, const uint8_t *data, size_t length);

/**
 * Initialize one UART BSP instance. Pass BSP_UART_DMA_CHANNEL_NONE for an
 * RX-only/blocking-TX instance.
 */
bool bsp_uart_init(bsp_uart_e device, uint8_t tx_dma_ch);

bool bsp_uart_send(
    bsp_uart_e device, const uint8_t *data, uint32_t length);
bool bsp_uart_send_async(
    bsp_uart_e device, const uint8_t *data, uint32_t length);
/* Blocking send and both formatted-output APIs require task context. Blocking
 * send returns false while asynchronous packets are pending. Formatted output
 * is rejected rather than truncated when it does not fit the TX buffer.
 */
bool bsp_uart_printf(bsp_uart_e device, const char *fmt, ...);
bool bsp_uart_printf_async(bsp_uart_e device, const char *fmt, ...);

/**
 * Select when the receive callback is invoked.
 *
 * TIMEOUT is the default and uses the one-shot UART_RX_IDLE timer. The
 * callback runs about 50-100 us after the last byte with the current SysConfig.
 * DELIMITER frames include the delimiter bytes. FIXED_LENGTH can emit several
 * frames from one FIFO drain. RAW emits one frame for every FIFO drain.
 */
bool bsp_uart_configure_rx(
    bsp_uart_e device, const bsp_uart_rx_config_t *config);

/**
 * Set the single receive callback for one UART. Passing NULL removes it.
 * The callback runs in interrupt context and receives complete frames
 * according to the configured RX mode. data is valid only until the callback
 * returns; copy it before deferring work to a task.
 */
void bsp_uart_set_callback(bsp_uart_e device, bsp_uart_callback_t callback);
bool bsp_uart_set_baudrate(bsp_uart_e device, uint32_t baudrate);

#ifdef __cplusplus
}
#endif
