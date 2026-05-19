//
// Created by fish on 2025/9/19.
//

#pragma once

#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} bsp_io_t;

typedef enum { IO_RESET = 0, IO_SET = 1 } bsp_io_state_e;

void bsp_io_set(bsp_io_t io);

void bsp_io_reset(bsp_io_t io);

void bsp_io_toggle(bsp_io_t io);

void bsp_io_write(bsp_io_t io, bsp_io_state_e state);

bsp_io_state_e bsp_io_read(bsp_io_t io);

#ifdef __cplusplus
}
#endif