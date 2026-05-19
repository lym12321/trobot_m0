cmake_minimum_required(VERSION 3.22)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

set(MCU_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft")
set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS_INIT}")
set(CMAKE_CXX_FLAGS_INIT "${MCU_FLAGS_INIT}")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS_INIT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS_INIT} -specs=nosys.specs")

set(CMAKE_PROJECT_NAME trobot)
project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_compile_options(
        -ffunction-sections -fdata-sections -fno-common -fmessage-length=0
        $<$<CONFIG:Debug>:-Og> $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:MinSizeRel>:-Os>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
)

add_compile_definitions(
        DeviceFamily_MSPM0G350X
        __MSPM0G3507__
        ARM_MATH_CM0PLUS
        $<$<CONFIG:Debug>:DEBUG>
)

add_subdirectory(core)
add_subdirectory(bsp)
add_subdirectory(components)
add_subdirectory(app)

add_executable(${CMAKE_PROJECT_NAME})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf" LINKER_LANGUAGE CXX)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -Wl,--no-warn-rwx-segments
        -Wl,-Map=${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        app
        components
        bsp
        ti_core
        m
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        COMMENT "Generating size, hex and bin files"
)

add_custom_target(flash_and_verify
        COMMAND openocd
        -f ${CMAKE_CURRENT_LIST_DIR}/daplink.cfg
        -c "init"
        -c "reset halt"
        -c "program $<TARGET_FILE:${CMAKE_PROJECT_NAME}> verify reset exit"
        DEPENDS ${CMAKE_PROJECT_NAME}
        COMMENT "Flashing and verifying firmware via OpenOCD"
        USES_TERMINAL
)