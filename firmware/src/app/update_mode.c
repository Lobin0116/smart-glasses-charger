#ifdef HIL_TEST
#include "update_mode.h"

#include <stdbool.h>

#include "button.h"
#include "gd32e23x.h"
#include "hal_usart.h"
#include "hal_wwdgt.h"
#include "state_machine.h"

extern sm_ctx_t sm;

#define SYSTEM_MEMORY_BASE 0x1FFFF000U
#define CMD_BUF_SIZE 16U

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

static bool str_eq(const char *a, const char *b, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void send_ack(const uint8_t *s, uint16_t len) {
    hal_usart_send(s, len);
}

void update_mode_poll(void) {
    static char buf[CMD_BUF_SIZE];
    static uint8_t idx = 0;
    uint8_t c;

    while (hal_usart_rx_peek(&c)) {
        if (c == '\n' || c == '\r') {
            (void)hal_usart_rx_get(&c);
            static const uint8_t ok_open[] = "OK_OPEN\n";
            static const uint8_t ok_close[] = "OK_CLOSE\n";
            static const uint8_t ok_key[] = "OK_KEY\n";
            if (idx == 6U && str_eq(buf, "UPDATE", 6U)) {
                jump_to_system_bootloader();
            } else if (idx == 4U && str_eq(buf, "OPEN", 4U)) {
                send_ack(ok_open, 8U);
                sm_inject_lid_event(true);
            } else if (idx == 5U && str_eq(buf, "CLOSE", 5U)) {
                send_ack(ok_close, 9U);
                sm_inject_lid_event(false);
            } else if (idx == 3U && str_eq(buf, "KEY", 3U)) {
                send_ack(ok_key, 7U);
                button_on_press();
            } else if (idx == 5U && str_eq(buf, "RESET", 5U)) {
                static const uint8_t ok_reset[] = "OK_RESET\n";
                send_ack(ok_reset, 9U);
                sm.state = ST_IDLE;
                sm.retry_count = 0U;
                sm.glass_present = false;
                sm.glass_charging = false;
                sm.glass_full = false;
            }
            idx = 0U;
        } else if (c >= 'A' && c <= 'Z' && idx < CMD_BUF_SIZE) {
            (void)hal_usart_rx_get(&c);
            buf[idx++] = (char)c;
        } else {
            break;
        }
    }
    hal_wwdgt_feed();
}
#endif
