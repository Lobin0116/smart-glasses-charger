#include "gd32e23x.h"

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
#include "power_mgmt.h"
#include "state_machine.h"

static sm_ctx_t sm;
static led_effect_ctx_t led_ctx;

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
    led_effect_init(&led_ctx);
    button_init();
    hal_pwr_idle();

    hal_fwdgt_feed();

    while (1) {
        sm_tick(&sm);
        button_poll();
        led_effect_poll(&led_ctx);
        hal_fwdgt_feed();
    }
}
