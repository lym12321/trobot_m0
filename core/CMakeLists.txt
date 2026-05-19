set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CORE_SRC ${CORE_DIR}/src)
set(CMSIS_ROOT ${CORE_SRC}/third_party/CMSIS)

set(LINKER_SCRIPT ${CORE_DIR}/device_linker.lds)
set(STARTUP_FILE ${CORE_SRC}/startup_mspm0g350x_gcc.c)

add_library(ti_core OBJECT
        ${STARTUP_FILE}
        ${CORE_DIR}/ti_msp_dl_config.c
        ${CORE_DIR}/syscalls.c
)

target_include_directories(ti_core PUBLIC
        ${CORE_DIR}
        ${CORE_SRC}
        ${CMSIS_ROOT}/Core/Include
)

target_link_options(ti_core INTERFACE
        -T${LINKER_SCRIPT}
)

set(TI_DRIVERLIB_LIB ${CORE_SRC}/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a)
add_library(ti_driverlib STATIC IMPORTED GLOBAL)
set_target_properties(ti_driverlib PROPERTIES IMPORTED_LOCATION ${TI_DRIVERLIB_LIB})
target_include_directories(ti_driverlib INTERFACE
        ${CORE_SRC}
        ${CMSIS_ROOT}/Core/Include
)

set(CMSIS_DSP_LIB ${CMSIS_ROOT}/DSP/lib/gcc/m0p/arm_cortexM0l_math.a)
add_library(cmsis_dsp STATIC IMPORTED GLOBAL)
set_target_properties(cmsis_dsp PROPERTIES IMPORTED_LOCATION ${CMSIS_DSP_LIB})
target_include_directories(cmsis_dsp INTERFACE
        ${CMSIS_ROOT}/Core/Include
        ${CMSIS_ROOT}/DSP/Include
        ${CMSIS_ROOT}/DSP/PrivateInclude
)

target_link_libraries(ti_core PUBLIC
        ti_driverlib
        cmsis_dsp
)