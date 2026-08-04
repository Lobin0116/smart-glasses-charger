#include "gd32e23x.h"

#include <stddef.h>

#include "aux_logic.h"
#include "button.h"
#include "charge_flow.h"
#include "cw2017.h"
#include "hal_exti.h"
#include "hal_wwdgt.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "hal_usart.h"
#include "ip5353.h"
#include "led.h"
#include "led_effect.h"
#include "mt5706.h"
#include "power_mgmt.h"
#include "state_machine.h"
#ifdef HIL_TEST
#include "update_mode.h"
#endif

#define SOC_REFRESH_MS 5000U
#define EXTI_DEBOUNCE_MS 20U

led_effect_ctx_t g_led_ctx;

#ifdef HIL_TEST
sm_ctx_t sm;
#else
static sm_ctx_t sm;
#endif
static uint32_t last_soc_refresh;
static volatile uint8_t exti_pending;
static volatile uint32_t exti_last_trigger_ms[16];

static void exti_callback(uint8_t line) {
    if (line < 16U) {
        exti_pending |= (uint8_t)(1U << line);
        exti_last_trigger_ms[line] = hal_timer_get_ms();
    }
}

static void process_exti_events(void) {
    if (exti_pending == 0U) {
        return;
    }
    uint32_t now = hal_timer_get_ms();

    if ((exti_pending & (1U << HAL_EXTI_LINE_HALL)) &&
        (now - exti_last_trigger_ms[HAL_EXTI_LINE_HALL] >= EXTI_DEBOUNCE_MS)) {
        exti_pending &= (uint8_t)~(1U << HAL_EXTI_LINE_HALL);
        sm_handle_event(&sm, HAL_EXTI_LINE_HALL);
    }

    if ((exti_pending & (1U << HAL_EXTI_LINE_KEY)) &&
        (now - exti_last_trigger_ms[HAL_EXTI_LINE_KEY] >= EXTI_DEBOUNCE_MS)) {
        exti_pending &= (uint8_t)~(1U << HAL_EXTI_LINE_KEY);
        button_on_press();
    }
}

static void refresh_case_status(void) {
    uint8_t soc = cw2017_get_soc();
    sm.case_soc = soc;

    bool charging = ip5353_is_charging();
    bool input_valid = ip5353_is_input_valid();
    bool full = ip5353_is_full();

    charge_arbitrate(input_valid, mt5706_has_event());

    led_effect_set_case_info(&g_led_ctx, soc, charging || input_valid, full);
}

void board_init(void) {
    hal_gpio_init();
    hal_timer_init();
    hal_i2c_init();
    hal_usart_init();
    hal_exti_init();
    hal_exti_register_callback(exti_callback);
    hal_wwdgt_init(20);
    led_init();
    cw2017_init();
}

#ifdef HIL_TEST
/* Temporary charge-performance verification harness for wireless charging tests.
 * Bypasses the state machine and continuously reads CW2017 + IP5353 registers,
 * USART-printing every 500ms so charge voltage trend can be observed.
 * Revert: delete this whole #ifdef HIL_TEST block and keep only the #else main. */
static uint8_t hex_digit(uint8_t v) {
    return (uint8_t)(v < 10U ? ('0' + v) : ('A' + v - 10U));
}

static uint16_t put_u16_dec(uint8_t *p, uint16_t v) {
    uint8_t buf[5];
    uint16_t i = 0U;
    uint16_t n = 0U;
    if (v == 0U) {
        p[0] = '0';
        return 1U;
    }
    while (v > 0U) {
        buf[i++] = (uint8_t)('0' + (v % 10U));
        v /= 10U;
    }
    while (i > 0U) {
        p[n++] = buf[--i];
    }
    return n;
}

int main(void) {
    board_init();
    mt5706_enable();
    for (volatile uint32_t d = 0U; d < 1000000U; d++) {
        hal_wwdgt_feed();
    }
    static uint8_t msg[56];
    while (1) {
        uint8_t ver = 0U, soc = 0U, vh = 0U, vl = 0U;
        uint8_t s0 = 0U, s2 = 0U, s5 = 0U;
        int rv = hal_i2c_read_reg(0x63U, 0x00U, &ver, 1U);
        int rs = hal_i2c_read_reg(0x63U, 0x04U, &soc, 1U);
        int rh = hal_i2c_read_reg(0x63U, 0x02U, &vh, 1U);
        int rl = hal_i2c_read_reg(0x63U, 0x03U, &vl, 1U);
        int r0 = hal_i2c_read_reg(0x75U, 0x45U, &s0, 1U);
        int r2 = hal_i2c_read_reg(0x75U, 0x50U, &s2, 1U);
        int r5 = hal_i2c_read_reg(0x75U, 0x69U, &s5, 1U);
        uint16_t raw = (uint16_t)(((uint16_t)(vh & 0x3FU) << 8) | vl);
        uint16_t mv = (uint16_t)(((uint32_t)raw * 5U) / 16U);
        uint16_t n = 0U;
        msg[n++] = 'V'; msg[n++] = ':';
        if (rv == 0) {
            msg[n++] = hex_digit((uint8_t)(ver >> 4));
            msg[n++] = hex_digit((uint8_t)(ver & 0xFU));
        } else {
            msg[n++] = '-';
        }
        msg[n++] = ' ';
        msg[n++] = 'S'; msg[n++] = ':';
        if (rs == 0) {
            msg[n++] = hex_digit((uint8_t)(soc >> 4));
            msg[n++] = hex_digit((uint8_t)(soc & 0xFU));
        } else {
            msg[n++] = '-';
        }
        msg[n++] = ' ';
        msg[n++] = 'B'; msg[n++] = ':';
        if (rh == 0 && rl == 0) {
            n += put_u16_dec(msg + n, mv);
            msg[n++] = 'm';
            msg[n++] = 'V';
        } else {
            msg[n++] = '-';
        }
        msg[n++] = ' ';
        msg[n++] = '4'; msg[n++] = '5'; msg[n++] = (r0 == 0) ? '+' : '-';
        msg[n++] = hex_digit((uint8_t)(s0 >> 4));
        msg[n++] = hex_digit((uint8_t)(s0 & 0xFU));
        msg[n++] = ' ';
        msg[n++] = '5'; msg[n++] = '0'; msg[n++] = (r2 == 0) ? '+' : '-';
        msg[n++] = hex_digit((uint8_t)(s2 >> 4));
        msg[n++] = hex_digit((uint8_t)(s2 & 0xFU));
        msg[n++] = ' ';
        msg[n++] = '6'; msg[n++] = '9'; msg[n++] = (r5 == 0) ? '+' : '-';
        msg[n++] = hex_digit((uint8_t)(s5 >> 4));
        msg[n++] = hex_digit((uint8_t)(s5 & 0xFU));
        msg[n++] = ' ';
        msg[n++] = 'M'; msg[n++] = ':';
        msg[n++] = (hal_i2c_write(0x2BU, NULL, 0U) == 0) ? 'O' : 'N';
        msg[n++] = '\n';
        hal_usart_send(msg, n);
        for (uint32_t s = 0U; s < 70U; s++) {
            for (volatile uint32_t d = 0U; d < 100000U; d++) {
            }
            hal_wwdgt_feed();
        }
    }
}
#else
int main(void) {
    board_init();
    sm_init(&sm);
    led_effect_init(&g_led_ctx);
    button_init();
    hal_pwr_idle();

    refresh_case_status();
    last_soc_refresh = hal_timer_get_ms();

    hal_wwdgt_feed();

    while (1) {
        process_exti_events();
        sm_tick(&sm);
        button_poll();
        led_effect_poll(&g_led_ctx);

        if (hal_timer_expired(last_soc_refresh, SOC_REFRESH_MS)) {
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }

        hal_wwdgt_feed();
    }
}
#endif
