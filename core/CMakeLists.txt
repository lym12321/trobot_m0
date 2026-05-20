add_library(ti_core OBJECT
        ${CMAKE_CURRENT_SOURCE_DIR}/src/startup_mspm0g350x_gcc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/syscalls.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ti_msp_dl_config.c

        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/event_groups.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/list.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/queue.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/stream_buffer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/tasks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/timers.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/dpl/AppHooks_freertos.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/dpl/StaticAllocs_freertos.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/portable/GCC/ARM_CM0/port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/portable/GCC/ARM_CM0/portasm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/portable/MemMang/heap_4.c
)

target_include_directories(ti_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/Core/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos/portable/GCC/ARM_CM0
)

target_link_options(ti_core INTERFACE
        -T${CMAKE_CURRENT_SOURCE_DIR}/device_linker.lds
)

add_library(ti_driverlib STATIC IMPORTED GLOBAL)
set_target_properties(ti_driverlib PROPERTIES
        IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a
)
target_include_directories(ti_driverlib INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/Core/Include
)

add_library(cmsis_dsp STATIC IMPORTED GLOBAL)
set_target_properties(cmsis_dsp PROPERTIES
        IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/DSP/lib/gcc/m0p/arm_cortexM0l_math.a
)
target_include_directories(cmsis_dsp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/Core/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/DSP/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/CMSIS/DSP/PrivateInclude
)

target_link_libraries(ti_core PUBLIC
        ti_driverlib
        cmsis_dsp
)
