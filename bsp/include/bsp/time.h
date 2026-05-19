//
// Created by fish on 2026/5/14.
//

#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_time_delay(uint32_t ms);
uint32_t bsp_time_get_ms();

#ifdef __cplusplus
}
#endif