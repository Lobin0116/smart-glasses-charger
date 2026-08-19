/* OTA Bootloader: copy Staging B → App slot A on reset, then jump to App. */

#include <stdbool.h>
#include <stdint.h>

#include "gd32e23x.h"
#include "hal_bootmeta.h"
#include "hal_flash.h"

#define BL_PAGE_SIZE HAL_FLASH_PAGE_SIZE

static void jump_to_app(void)
{
    uint32_t app_base = BOOT_APP_BASE;
    uint32_t sp = *(volatile uint32_t *)app_base;
    uint32_t reset = *(volatile uint32_t *)(app_base + 4U);
    SCB->VTOR = app_base;
    __set_MSP(sp);
    __enable_irq(); /* App's startup doesn't re-enable IRQ */
    ((void (*)(void))reset)();
    while (1) {
    }
}

static bool copy_staging_to_app(uint32_t fw_size)
{
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

/* Nail the active-low LED bank (RED PB8 / GREEN PB9 / WS2812 PB2 / BLUE PF6 /
 * WHITE PF7) to push-pull HIGH = off, in the first instructions of BL.
 *
 * After POR every GPIO floats, and a floating active-low LED glows faintly —
 * seen as the brief red/blue flash on plug-in. The App's hal_gpio_init
 * re-configures the same pins later; this only shrinks the undefined window
 * from the whole boot chain (plus up to ~3 s of OTA staging copy, during
 * which the LEDs would also float) down to the tens of us before BL main.
 *
 * Raw register writes: pulling in the SPL gpio/rcu sources would cost >1 KB
 * of the 4 KB BL slot for five one-shot stores. ODR is written via BOP while
 * the pins are still in reset-default input mode, so the CTL switch to output
 * never drives the ODR reset value (0 = LED on). OMODE/PUD reset values
 * (push-pull, no pull) already match what we want. */
static void leds_force_off(void)
{
    RCU_AHBEN |= RCU_AHBEN_PBEN | RCU_AHBEN_PFEN;
    GPIO_BOP(GPIOB) = GPIO_PIN_2 | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_BOP(GPIOF) = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_CTL(GPIOB) |= (0x1U << 4) | (0x1U << 16) | (0x1U << 18); /* PB2/PB8/PB9 -> output */
    GPIO_CTL(GPIOF) |= (0x1U << 12) | (0x1U << 14);               /* PF6/PF7 -> output */
}

int main(void)
{
    leds_force_off();

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
