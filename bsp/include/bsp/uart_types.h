#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_UART_RX_DELIMITER_MAX_SIZE 8u

typedef enum {
    BSP_UART_RX_MODE_RAW = 0,
    BSP_UART_RX_MODE_TIMEOUT,
    BSP_UART_RX_MODE_FIXED_LENGTH,
    BSP_UART_RX_MODE_DELIMITER,
} bsp_uart_rx_mode_e;

typedef struct {
    bsp_uart_rx_mode_e mode;
    /* Required only in FIXED_LENGTH mode; valid range is 1..RX buffer size. */
    uint16_t fixed_length;
    /* Copied by the BSP in DELIMITER mode; delimiter bytes are kept in frames. */
    const uint8_t *delimiter;
    uint8_t delimiter_length;
} bsp_uart_rx_config_t;

#ifdef __cplusplus
}
#endif
