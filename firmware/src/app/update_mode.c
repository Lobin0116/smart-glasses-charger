#include "update_mode.h"

#include "gd32e23x.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_wwdgt.h"

#define SYSTEM_MEMORY_BASE 0x1FFFF000U

typedef void (*pfunc_t)(void);

static void jump_to_system_bootloader(void) {
    uint32_t addr = SYSTEM_MEMORY_BASE;

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    usart_disable(USART0);

    __DSB();
    __ISB();

    __set_MSP(*(volatile uint32_t *)addr);

    pfunc_t reset = (pfunc_t)(*(volatile uint32_t *)(addr + 4));
    reset();

    while (1) {
    }
}

void update_mode_wait(uint32_t timeout_ms) {
    uint32_t start = hal_timer_get_ms();
    char buf[8];
    uint8_t idx = 0;

    hal_tr_switch_set(false);

    while (!hal_timer_expired(start, timeout_ms)) {
        if (usart_flag_get(USART0, USART_FLAG_RBNE) != RESET) {
            char c = (char)usart_data_receive(USART0);
            if (c == '\n' || c == '\r') {
                if (idx == 6 && buf[0] == 'U' && buf[1] == 'P' && buf[2] == 'D' &&
                    buf[3] == 'A' && buf[4] == 'T' && buf[5] == 'E') {
                    jump_to_system_bootloader();
                }
                idx = 0;
            } else if (c >= 'A' && c <= 'Z' && idx < sizeof(buf)) {
                buf[idx++] = c;
            }
        }
        hal_wwdgt_feed();
    }
}
