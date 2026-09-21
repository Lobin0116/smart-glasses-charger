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
#include "nu1671.h"
#include "power_mgmt.h"
#include "state_machine.h"
#ifdef HIL_TEST
    #include "update_mode.h"
#endif

#define SOC_REFRESH_MS 500U

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
static volatile bool exti_woken;

static void exti_callback(uint8_t line)
{
    if (line < 16U) {

        if (line == HAL_EXTI_LINE_KEY) {
            uint8_t mask = (uint8_t)(1U << line);
            if ((exti_pending & mask) == 0U) {
                exti_last_trigger_ms[line] = hal_timer_get_ms();
            }
            exti_pending |= mask;
        } else if (line == HAL_EXTI_LINE_HALL) {

            sm.hall_edge_seen = true;
        } else if (line == HAL_EXTI_LINE_NINT) {

            nu1671_on_interrupt();
        }
        exti_woken = true;
    }
}

static void process_exti_events(void)
{
    if (exti_pending == 0U) {
        return;
    }
    uint32_t now = hal_timer_get_ms();

    if ((exti_pending & (1U << HAL_EXTI_LINE_KEY))
        && (now - exti_last_trigger_ms[HAL_EXTI_LINE_KEY] >= EXTI_DEBOUNCE_MS)) {
        exti_pending &= (uint8_t)~(1U << HAL_EXTI_LINE_KEY);
        button_on_press();
    }

    exti_pending &= (uint8_t)~((1U << HAL_EXTI_LINE_CHARGER_INT)
                               | (1U << HAL_EXTI_LINE_BAT_INT)
                               | (1U << HAL_EXTI_LINE_NINT));
}

static void refresh_case_status(void)
{
    uint8_t soc = cw2017_get_soc();
    sm.case_soc = soc;
    sm.ntc_temp_c = cw2017_get_temp_c();

    bool charging = ip5353_is_charging();
    bool input_valid = ip5353_is_input_valid();
    bool full = ip5353_is_full();

    if (charging) {
        hal_boost_5v_disable();
    } else {
        hal_boost_5v_enable();
    }

    (void)nu1671_poll();

    led_effect_set_case_info(&g_led_ctx, soc, charging || input_valid, full);
}

void board_init(void)
{
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

int main(void)
{
    board_init();
    sm_init(&sm);
    led_effect_init(&g_led_ctx);
    button_init();
    hal_pwr_idle();

    hal_power_gate_on(HAL_POWER_GATE_POGO3V3);

    hal_power_gate_on(HAL_POWER_GATE_BAT);

    hal_boost_5v_enable();

    hal_1v8_enable();

    exti_pending = 0U;
#ifdef HIL_TEST

    exti_interrupt_disable(EXTI_4);
    exti_interrupt_flag_clear(EXTI_4);
#endif

    hal_timer_delay_ms(500);

    refresh_case_status();
    last_soc_refresh = hal_timer_get_ms();

    hal_wwdgt_feed();

    while (1) {

        if (exti_woken) {
            exti_woken = false;
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }
        process_exti_events();
#ifdef HIL_TEST

        update_mode_poll();
#endif
        sm_tick(&sm);
        button_poll();
        led_effect_poll(&g_led_ctx);

        if (hal_timer_expired(last_soc_refresh, SOC_REFRESH_MS)) {
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }

        hal_wwdgt_feed();

#ifndef HIL_TEST
        if (sm_can_sleep(&sm) && exti_pending == 0U && !hal_charger_int_get()) {
            pm_enter_deep_sleep();
        }
#endif
    }
}
