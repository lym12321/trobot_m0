#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/uart_types.h"

typedef void (*bsp_uart_rx_emit_t)(
    void *context, const uint8_t *data, size_t length);

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t length;
    bsp_uart_rx_config_t config;
    uint8_t delimiter[BSP_UART_RX_DELIMITER_MAX_SIZE];
    uint8_t delimiter_prefix[BSP_UART_RX_DELIMITER_MAX_SIZE];
    uint8_t delimiter_match;
    bool discarding;
    bsp_uart_rx_emit_t emit;
    void *emit_context;
} bsp_uart_rx_processor_t;

bool bsp_uart_rx_processor_init(bsp_uart_rx_processor_t *processor,
                                uint8_t *buffer, size_t capacity,
                                bsp_uart_rx_emit_t emit, void *emit_context);
bool bsp_uart_rx_processor_configure(
    bsp_uart_rx_processor_t *processor, const bsp_uart_rx_config_t *config);
void bsp_uart_rx_processor_feed(bsp_uart_rx_processor_t *processor,
                                const uint8_t *data, size_t length);
void bsp_uart_rx_processor_timeout(bsp_uart_rx_processor_t *processor);
void bsp_uart_rx_processor_error(bsp_uart_rx_processor_t *processor);
