#include "gd32e23x.h"

#include "aux_logic.h"
#include "button.h"
#include "charge_flow.h"
#include "cw2017.h"
#include "hal_exti.h"
#include "hal_fwdgt.h"
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

#define SOC_REFRESH_MS 5000U

led_effect_ctx_t g_led_ctx;

static sm_ctx_t sm;
static uint32_t last_soc_refresh;

static void exti_callback(uint8_t line) {
    switch (line) {
    case HAL_EXTI_LINE_HALL:
        sm_handle_event(&sm, line);
        break;
    case HAL_EXTI_LINE_KEY:
        button_on_press();
        break;
    default:
        break;
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

void SystemInit(void) {
    hal_gpio_init();
    hal_timer_init();
    hal_i2c_init();
    hal_usart_init();
    hal_exti_init();
    hal_exti_register_callback(exti_callback);
    hal_fwdgt_init(2000);
    led_init();
    cw2017_init();
}

int main(void) {
    sm_init(&sm);
    led_effect_init(&g_led_ctx);
    button_init();
    hal_pwr_idle();

    refresh_case_status();
    last_soc_refresh = hal_timer_get_ms();

    hal_fwdgt_feed();

    while (1) {
        sm_tick(&sm);
        button_poll();
        led_effect_poll(&g_led_ctx);

        if (hal_timer_expired(last_soc_refresh, SOC_REFRESH_MS)) {
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }

        hal_fwdgt_feed();
    }
}
