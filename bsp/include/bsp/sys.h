//
// Created by fish on 2026/1/8.
//

#pragma once

#include <stdbool.h>

#include "bsp/def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 判断是否在中断中
 * @return 若在中断中调用则返回 true, 否则返回 false
 */
bool bsp_sys_in_isr(void);

/**
 * 软件 reset, 效果几乎等效于按 reset 键
 */
void bsp_sys_reset(void);

/**
 * 进入临界区
 * @return state, 在 exit 时需传入
 */
unsigned long bsp_sys_enter_critical(void);

/**
 * 退出临界区
 * @param state 进入时获取的 state
 */
void bsp_sys_exit_critical(unsigned long state);

#ifdef __cplusplus
}
#endif
