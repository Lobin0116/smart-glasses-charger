# Bootloader sub-project: 4 KB image at 0x08000000 that copies staged
# firmware from Staging B → App slot A on reset, then jumps to App.
# Shares source with App (startup, CMSIS system, hal_flash, hal_bootmeta).
# See docs/OTA_UPGRADE_PLAN.md.

cmake_minimum_required(VERSION 3.13)

set(MCU_FLAGS "-mcpu=cortex-m23 -mthumb")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS} -std=c11 -Wall -Wextra -ffunction-sections -fdata-sections -gdwarf-2")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS} -x assembler-with-cpp")

set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CMAKE_EXE_LINKER_FLAGS
    "${MCU_FLAGS} -nostartfiles -T${CMAKE_CURRENT_SOURCE_DIR}/gd32e230x8_bl.ld -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/bootloader.map --specs=nano.specs --specs=nosys.specs")

set(CMSIS_INC
    ${FIRMWARE_DIR}/Drivers/CMSIS
    ${FIRMWARE_DIR}/Drivers/CMSIS/Include
)
set(SPL_INC ${FIRMWARE_DIR}/Drivers/SPL)

# Only the SPL pieces BL touches: FMC (flash erase/program). BL doesn't use
# RCU API (no clock config in lite SystemInit), WWDGT, or NVIC.
set(BL_SPL_SRC
    ${FIRMWARE_DIR}/Drivers/SPL/gd32e23x_fmc.c
)

add_executable(bootloader.elf
    ${FIRMWARE_DIR}/startup/startup_gd32e23x.S
    ${CMAKE_CURRENT_SOURCE_DIR}/system_gd32e23x_lite.c
    ${FIRMWARE_DIR}/src/hal/hal_flash.c
    ${FIRMWARE_DIR}/src/hal/hal_bootmeta.c
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${BL_SPL_SRC}
)

target_include_directories(bootloader.elf PRIVATE
    ${CMSIS_INC}
    ${SPL_INC}
    ${FIRMWARE_DIR}/src
    ${FIRMWARE_DIR}/src/hal
)

target_compile_definitions(bootloader.elf PRIVATE
    GD32E230
    USE_STDPERIPH_DRIVER
    VECT_TAB_OFFSET=0x00U
    NO_LIBC_INIT_ARRAY
    BL_NO_WWDGT
)

add_custom_command(TARGET bootloader.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:bootloader.elf> ${CMAKE_BINARY_DIR}/bootloader.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader.elf> ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:bootloader.elf>
)
