/* OTA Bootloader: copy Staging B → App slot A on reset, then jump to App. */

#include <stdbool.h>
#include <stdint.h>

#include "gd32e23x.h"
#include "hal_bootmeta.h"
#include "hal_flash.h"

#define BL_PAGE_SIZE  HAL_FLASH_PAGE_SIZE

static void jump_to_app(void) {
    uint32_t app_base = BOOT_APP_BASE;
    uint32_t sp = *(volatile uint32_t *)app_base;
    uint32_t reset = *(volatile uint32_t *)(app_base + 4U);
    SCB->VTOR = app_base;
    __set_MSP(sp);
    __enable_irq();  /* App's startup doesn't re-enable IRQ */
    ((void (*)(void))reset)();
    while (1) {
    }
}

static bool copy_staging_to_app(uint32_t fw_size) {
    uint32_t pages = (fw_size + BL_PAGE_SIZE - 1U) / BL_PAGE_SIZE;
    hal_flash_unlock();
    for (uint32_t p = 0U; p < pages; p++) {
        if (!hal_flash_page_erase(BOOT_APP_BASE + p * BL_PAGE_SIZE)) {
            hal_flash_lock();
            return false;
        }
    }
    uint32_t words = (fw_size + 3U) / 4U;
    for (uint32_t w = 0U; w < words; w++) {
        uint8_t buf[4];
        const volatile uint8_t *src = (const volatile uint8_t *)(BOOT_STAGING_BASE + w * 4U);
        buf[0] = src[0];
        buf[1] = src[1];
        buf[2] = src[2];
        buf[3] = src[3];
        if (!hal_flash_write(BOOT_APP_BASE + w * 4U, buf, 4U)) {
            hal_flash_lock();
            return false;
        }
    }
    hal_flash_lock();
    return true;
}

int main(void) {
    bool staged = false;
    uint32_t fw_size = 0U;
    (void)hal_bootmeta_read_staged(&staged, &fw_size);

    if (staged && fw_size > 0U && fw_size <= BOOT_STAGING_SIZE) {
        if (copy_staging_to_app(fw_size)) {
            (void)hal_bootmeta_clear_staged();
        }
    }

    jump_to_app();
    return 0;
}
