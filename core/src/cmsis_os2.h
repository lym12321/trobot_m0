//
// CMSIS-RTOS2 最小兼容层 for MSPM0 (FreeRTOS)
// 只提供 os.h 需要的优先级常量，不做完整 RTOS 抽象
//

#pragma once

#include "FreeRTOS.h"

// CMSIS-RTOS2 任务优先级常量
// 直接映射到 FreeRTOS configMAX_PRIORITIES 的比例位置
#define osPriorityIdle     0
#define osPriorityLow      ((configMAX_PRIORITIES) / 4)
#define osPriorityNormal   ((configMAX_PRIORITIES) / 2)
#define osPriorityHigh     ((configMAX_PRIORITIES) * 3 / 4)
#define osPriorityRealtime ((configMAX_PRIORITIES) - 1)
