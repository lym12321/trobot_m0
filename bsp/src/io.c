//
// Created by fish on 2025/9/19.
//

#include "bsp/io.h"

#include "ti/driverlib/dl_gpio.h"

void bsp_io_set(bsp_io_t io) {
    DL_GPIO_setPins(io.port, io.pin);
}

void bsp_io_reset(bsp_io_t io) {
    DL_GPIO_clearPins(io.port, io.pin);
}

void bsp_io_toggle(bsp_io_t io) {
    DL_GPIO_togglePins(io.port, io.pin);
}

void bsp_io_write(bsp_io_t io, bsp_io_state_e state) {
    state == IO_RESET ? bsp_io_reset(io) : bsp_io_set(io);
}

bsp_io_state_e bsp_io_read(bsp_io_t io) {
    return DL_GPIO_readPins(io.port, io.pin) == 0 ? IO_RESET : IO_SET;
}