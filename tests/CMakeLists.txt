cmake_minimum_required(VERSION 3.22)
project(trobot_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(uart_rx_test
    uart_rx_test.c
    ../bsp/src/uart_rx.c
)
target_include_directories(uart_rx_test PRIVATE
    ../bsp/include
    ../bsp/internal
)

add_test(NAME uart_rx_test COMMAND uart_rx_test)
