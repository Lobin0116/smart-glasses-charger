#include "gd32e23x.h"

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
#include "update_mode.h"

#define SOC_REFRESH_MS 5000U
#define EXTI_DEBOUNCE_MS 20U

led_effect_ctx_t g_led_ctx;

static sm_ctx_t sm;
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

int main(void) {
    board_init();
    update_mode_wait(2000);
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
