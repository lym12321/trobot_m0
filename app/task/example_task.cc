//
// Created by fish on 2026/5/21.
//

#include "bsp/time.h"

#include "utils/logger.h"
#include "utils/os.h"

void example_task(void *args) {
    for (;;) {
        logger::info("Hello, world! Time: %d ms", bsp_time_get_ms());
        os::task::sleep(1);
    }
}
