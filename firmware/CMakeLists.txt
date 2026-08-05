cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ARM_TOOLCHAIN_PREFIX)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_DEFAULT_PREFIX
            "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/13.2 Rel1/bin/arm-none-eabi")
    else()
        set(_DEFAULT_PREFIX "arm-none-eabi")
    endif()
    set(ARM_TOOLCHAIN_PREFIX ${_DEFAULT_PREFIX} CACHE STRING "ARM GCC toolchain prefix")
    unset(_DEFAULT_PREFIX)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_EXE_SUFFIX ".exe")
else()
    set(_EXE_SUFFIX "")
endif()

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m23)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER   ${ARM_TOOLCHAIN_PREFIX}-gcc${_EXE_SUFFIX})
set(CMAKE_ASM_COMPILER ${ARM_TOOLCHAIN_PREFIX}-gcc${_EXE_SUFFIX})
set(CMAKE_OBJCOPY      ${ARM_TOOLCHAIN_PREFIX}-objcopy${_EXE_SUFFIX} CACHE FILEPATH "")
set(CMAKE_OBJDUMP      ${ARM_TOOLCHAIN_PREFIX}-objdump${_EXE_SUFFIX} CACHE FILEPATH "")
set(CMAKE_SIZE         ${ARM_TOOLCHAIN_PREFIX}-size${_EXE_SUFFIX}    CACHE FILEPATH "")

project(smart_glasses_charger C ASM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(HIL_TEST "Enable HIL test hooks (USART OPEN/CLOSE/KEY/RESET commands, no Deep-Sleep in IDLE)" OFF)

set(MCU_FLAGS "-mcpu=cortex-m23 -mthumb")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS} -std=c11 -Wall -Wextra -ffunction-sections -fdata-sections -gdwarf-2")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MCU_FLAGS} -T${CMAKE_SOURCE_DIR}/startup/gd32e230x8_flash.ld -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map --specs=nano.specs --specs=nosys.specs")

set(CMSIS_INC
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)
set(SPL_INC ${CMAKE_SOURCE_DIR}/Drivers/SPL)

file(GLOB SPL_SRC ${CMAKE_SOURCE_DIR}/Drivers/SPL/*.c)
file(GLOB_RECURSE APP_SRC ${CMAKE_SOURCE_DIR}/src/*.c)

add_executable(${PROJECT_NAME}.elf
    ${CMAKE_SOURCE_DIR}/startup/startup_gd32e23x.S
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/system_gd32e23x.c
    ${SPL_SRC}
    ${APP_SRC}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMSIS_INC}
    ${SPL_INC}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/hal
    ${CMAKE_SOURCE_DIR}/src/protocol
    ${CMAKE_SOURCE_DIR}/src/drivers
    ${CMAKE_SOURCE_DIR}/src/app
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    GD32E230
    USE_STDPERIPH_DRIVER
    VECT_TAB_OFFSET=0x1000U
)

if(HIL_TEST)
    target_compile_definitions(${PROJECT_NAME}.elf PRIVATE HIL_TEST)
endif()

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
)

# Bootloader sub-project (4 KB at 0x08000000). Shares source with App.
add_subdirectory(bootloader)

# Merge Bootloader.hex + App.hex into a single combined.hex for one-shot flashing.
add_custom_target(combined_hex ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/merge_hex.py
            ${CMAKE_BINARY_DIR}/bootloader.hex
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
            ${CMAKE_BINARY_DIR}/combined.hex
    DEPENDS bootloader.elf ${PROJECT_NAME}.elf
    COMMENT "Merging bootloader.hex + ${PROJECT_NAME}.hex -> combined.hex"
    VERBATIM
)
