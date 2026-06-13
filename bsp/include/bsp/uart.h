//
// Created by fish on 2026/5/14.
//

#pragma once

#include "bsp/def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_UART_DEVICE_COUNT 4
#define BSP_UART_BUFFER_SIZE 128

typedef bsp_uart_handle_t bsp_uart_e;
typedef void (*bsp_uart_callback_t) (bsp_uart_e device, const uint8_t *data, size_t len);

void bsp_uart_init(bsp_uart_e device, uint8_t tx_dma_ch);
void bsp_uart_send(bsp_uart_e device, const uint8_t *data, uint32_t len);
void bsp_uart_send_async(bsp_uart_e device, const uint8_t *data, uint32_t len);
void bsp_uart_printf(bsp_uart_e device, const char *fmt, ...);
void bsp_uart_printf_async(bsp_uart_e device, const char *fmt, ...);
void bsp_uart_set_callback(bsp_uart_e device, bsp_uart_callback_t callback);
void bsp_uart_set_baudrate(bsp_uart_e device, uint32_t baudrate);

#ifdef __cplusplus
}
#endif
