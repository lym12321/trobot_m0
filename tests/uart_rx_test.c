#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bsp/uart_rx.h"

typedef struct {
    unsigned frame_count;
    size_t last_length;
    uint8_t last_data[32];
} test_events_t;

static void capture_frame(
    void *context, const uint8_t *data, size_t length) {
    test_events_t *events = context;

    events->frame_count++;
    events->last_length = length;
    assert(length <= sizeof(events->last_data));
    memcpy(events->last_data, data, length);
}

static void timeout_frame_test(void) {
    uint8_t frame_buffer[16];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_TIMEOUT,
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t first[] = {'a', 'b'};
    const uint8_t second[] = {'c', 'd'};
    bsp_uart_rx_processor_feed(&processor, first, sizeof(first));
    bsp_uart_rx_processor_feed(&processor, second, sizeof(second));
    assert(events.frame_count == 0);

    bsp_uart_rx_processor_timeout(&processor);
    assert(events.frame_count == 1);
    assert(events.last_length == 4);
    assert(memcmp(events.last_data, "abcd", 4) == 0);
}

static void delimiter_split_test(void) {
    uint8_t frame_buffer[16];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    static const uint8_t delimiter[] = {'\r', '\n'};
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_DELIMITER,
        .delimiter = delimiter,
        .delimiter_length = sizeof(delimiter),
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t first[] = {'O', 'K', '\r'};
    const uint8_t second[] = {'\n'};
    bsp_uart_rx_processor_feed(&processor, first, sizeof(first));
    assert(events.frame_count == 0);
    bsp_uart_rx_processor_feed(&processor, second, sizeof(second));

    assert(events.frame_count == 1);
    assert(events.last_length == 4);
    assert(memcmp(events.last_data, "OK\r\n", 4) == 0);
}

static void fixed_frame_test(void) {
    uint8_t frame_buffer[4];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_FIXED_LENGTH,
        .fixed_length = 3,
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t data[] = {'a', 'b', 'c', '1', '2', '3'};
    bsp_uart_rx_processor_feed(&processor, data, sizeof(data));

    assert(events.frame_count == 2);
    assert(events.last_length == 3);
    assert(memcmp(events.last_data, "123", 3) == 0);
}

static void delimiter_overflow_test(void) {
    uint8_t frame_buffer[5];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    static const uint8_t delimiter[] = {'\r', '\n'};
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_DELIMITER,
        .delimiter = delimiter,
        .delimiter_length = sizeof(delimiter),
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t data[] = "abcdef\r\nOK\r\n";
    bsp_uart_rx_processor_feed(&processor, data, sizeof(data) - 1);

    assert(events.frame_count == 1);
    assert(events.last_length == 4);
    assert(memcmp(events.last_data, "OK\r\n", 4) == 0);
}

static void delimiter_split_overflow_test(void) {
    uint8_t frame_buffer[4];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    static const uint8_t delimiter[] = {'\r', '\n'};
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_DELIMITER,
        .delimiter = delimiter,
        .delimiter_length = sizeof(delimiter),
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t data[] = "ABC\r\nOK\r\n";
    bsp_uart_rx_processor_feed(&processor, data, sizeof(data) - 1);

    assert(events.frame_count == 1);
    assert(events.last_length == 4);
    assert(memcmp(events.last_data, "OK\r\n", 4) == 0);
}

static void timeout_overflow_test(void) {
    uint8_t frame_buffer[4];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_TIMEOUT,
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t oversized[] = "abcdef";
    bsp_uart_rx_processor_feed(
        &processor, oversized, sizeof(oversized) - 1);
    bsp_uart_rx_processor_timeout(&processor);
    assert(events.frame_count == 0);

    const uint8_t valid[] = "OK";
    bsp_uart_rx_processor_feed(&processor, valid, sizeof(valid) - 1);
    bsp_uart_rx_processor_timeout(&processor);
    assert(events.frame_count == 1);
    assert(events.last_length == 2);
    assert(memcmp(events.last_data, "OK", 2) == 0);
}

static void raw_mode_test(void) {
    uint8_t frame_buffer[4];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_RAW,
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t first[] = "ab";
    const uint8_t second[] = "cd";
    bsp_uart_rx_processor_feed(&processor, first, sizeof(first) - 1);
    bsp_uart_rx_processor_feed(&processor, second, sizeof(second) - 1);

    assert(events.frame_count == 2);
    assert(events.last_length == 2);
    assert(memcmp(events.last_data, "cd", 2) == 0);
}

static void rx_error_test(void) {
    uint8_t frame_buffer[8];
    test_events_t events = {0};
    bsp_uart_rx_processor_t processor;
    const bsp_uart_rx_config_t config = {
        .mode = BSP_UART_RX_MODE_TIMEOUT,
    };

    assert(bsp_uart_rx_processor_init(
        &processor, frame_buffer, sizeof(frame_buffer), capture_frame, &events));
    assert(bsp_uart_rx_processor_configure(&processor, &config));

    const uint8_t data[] = "bad";
    bsp_uart_rx_processor_feed(&processor, data, sizeof(data) - 1);
    bsp_uart_rx_processor_error(&processor);
    bsp_uart_rx_processor_timeout(&processor);
    assert(events.frame_count == 0);

    const uint8_t valid[] = "OK";
    bsp_uart_rx_processor_feed(&processor, valid, sizeof(valid) - 1);
    bsp_uart_rx_processor_timeout(&processor);
    assert(events.frame_count == 1);
    assert(events.last_length == 2);
    assert(memcmp(events.last_data, "OK", 2) == 0);
}

int main(void) {
    timeout_frame_test();
    delimiter_split_test();
    fixed_frame_test();
    delimiter_overflow_test();
    delimiter_split_overflow_test();
    timeout_overflow_test();
    raw_mode_test();
    rx_error_test();
    return 0;
}
