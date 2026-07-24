#include "bsp/uart_rx.h"

#include <string.h>

static void emit_frame(bsp_uart_rx_processor_t *processor) {
    processor->emit(
        processor->emit_context, processor->buffer, processor->length);
}

static void reset_frame(bsp_uart_rx_processor_t *processor) {
    processor->length = 0;
    processor->delimiter_match = 0;
    processor->discarding = false;
}

static bool advance_delimiter(bsp_uart_rx_processor_t *processor,
                              uint8_t value) {
    while (processor->delimiter_match > 0 &&
           value != processor->delimiter[processor->delimiter_match]) {
        processor->delimiter_match =
            processor->delimiter_prefix[processor->delimiter_match - 1];
    }
    if (value == processor->delimiter[processor->delimiter_match]) {
        processor->delimiter_match++;
    }

    return processor->delimiter_match ==
           processor->config.delimiter_length;
}

static bool discard_until_delimiter(bsp_uart_rx_processor_t *processor,
                                    uint8_t value) {
    if (advance_delimiter(processor, value)) {
        reset_frame(processor);
        return true;
    }
    return false;
}

bool bsp_uart_rx_processor_init(bsp_uart_rx_processor_t *processor,
                                uint8_t *buffer, size_t capacity,
                                bsp_uart_rx_emit_t emit, void *emit_context) {
    if (processor == NULL || buffer == NULL || capacity == 0 || emit == NULL) {
        return false;
    }

    memset(processor, 0, sizeof(*processor));
    processor->buffer = buffer;
    processor->capacity = capacity;
    processor->emit = emit;
    processor->emit_context = emit_context;
    return true;
}

bool bsp_uart_rx_processor_configure(
    bsp_uart_rx_processor_t *processor, const bsp_uart_rx_config_t *config) {
    if (processor == NULL || config == NULL) {
        return false;
    }

    if (config->mode != BSP_UART_RX_MODE_RAW &&
        config->mode != BSP_UART_RX_MODE_TIMEOUT &&
        config->mode != BSP_UART_RX_MODE_FIXED_LENGTH &&
        config->mode != BSP_UART_RX_MODE_DELIMITER) {
        return false;
    }

    if (config->mode == BSP_UART_RX_MODE_FIXED_LENGTH &&
        (config->fixed_length == 0 ||
         config->fixed_length > processor->capacity)) {
        return false;
    }

    if (config->mode == BSP_UART_RX_MODE_DELIMITER &&
        (config->delimiter == NULL || config->delimiter_length == 0 ||
         config->delimiter_length > sizeof(processor->delimiter) ||
         config->delimiter_length > processor->capacity)) {
        return false;
    }

    processor->config = *config;
    if (config->mode == BSP_UART_RX_MODE_DELIMITER) {
        memcpy(processor->delimiter, config->delimiter,
               config->delimiter_length);
        processor->config.delimiter = processor->delimiter;
        processor->delimiter_prefix[0] = 0;
        uint8_t prefix_length = 0;
        for (uint8_t i = 1; i < config->delimiter_length; i++) {
            while (prefix_length > 0 &&
                   processor->delimiter[i] !=
                       processor->delimiter[prefix_length]) {
                prefix_length =
                    processor->delimiter_prefix[prefix_length - 1];
            }
            if (processor->delimiter[i] ==
                processor->delimiter[prefix_length]) {
                prefix_length++;
            }
            processor->delimiter_prefix[i] = prefix_length;
        }
    }
    reset_frame(processor);
    return true;
}

void bsp_uart_rx_processor_feed(bsp_uart_rx_processor_t *processor,
                                const uint8_t *data, size_t length) {
    if (processor == NULL || data == NULL || length == 0) {
        return;
    }

    if (processor->config.mode == BSP_UART_RX_MODE_RAW) {
        processor->emit(processor->emit_context, data, length);
        return;
    }

    for (size_t i = 0; i < length; i++) {
        if (processor->discarding) {
            if (processor->config.mode == BSP_UART_RX_MODE_DELIMITER) {
                (void)discard_until_delimiter(processor, data[i]);
            }
            continue;
        }

        if (processor->length == processor->capacity) {
            processor->length = 0;
            processor->discarding = true;
            if (processor->config.mode == BSP_UART_RX_MODE_DELIMITER) {
                (void)discard_until_delimiter(processor, data[i]);
            } else {
                processor->delimiter_match = 0;
            }
            continue;
        }
        processor->buffer[processor->length++] = data[i];

        if (processor->config.mode == BSP_UART_RX_MODE_DELIMITER &&
            advance_delimiter(processor, data[i])) {
            emit_frame(processor);
            reset_frame(processor);
        } else if (processor->config.mode ==
                       BSP_UART_RX_MODE_FIXED_LENGTH &&
                   processor->length == processor->config.fixed_length) {
            emit_frame(processor);
            reset_frame(processor);
        }
    }
}

void bsp_uart_rx_processor_timeout(bsp_uart_rx_processor_t *processor) {
    if (processor == NULL) {
        return;
    }

    if (processor->discarding &&
        processor->config.mode != BSP_UART_RX_MODE_DELIMITER) {
        reset_frame(processor);
    } else if (processor->length > 0 &&
               processor->config.mode == BSP_UART_RX_MODE_TIMEOUT) {
        emit_frame(processor);
        reset_frame(processor);
    }
}

void bsp_uart_rx_processor_error(bsp_uart_rx_processor_t *processor) {
    if (processor == NULL) {
        return;
    }

    processor->length = 0;
    processor->delimiter_match = 0;
    processor->discarding =
        processor->config.mode != BSP_UART_RX_MODE_RAW;
}
