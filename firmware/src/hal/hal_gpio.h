#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>

#include "hal_pinmux.h"

void hal_gpio_init(void);

void hal_gpio_set(hal_pin_t pin, bool value);
bool hal_gpio_get(hal_pin_t pin);
void hal_gpio_toggle(hal_pin_t pin);

void hal_led_red_on(void);
void hal_led_red_off(void);
void hal_led_red_toggle(void);
void hal_led_green_on(void);
void hal_led_green_off(void);
void hal_led_green_toggle(void);
void hal_led_blue_on(void);
void hal_led_blue_off(void);
void hal_led_blue_toggle(void);
void hal_led_white_on(void);
void hal_led_white_off(void);
void hal_led_white_toggle(void);

typedef enum
{
    HAL_POWER_GATE_POGO3V3,
    HAL_POWER_GATE_BAT,
    HAL_POWER_GATE_COUNT
} hal_power_gate_t;

void hal_power_gate_on(hal_power_gate_t gate);
void hal_power_gate_off(hal_power_gate_t gate);
bool hal_power_gate_is_on(hal_power_gate_t gate);

void hal_1v8_enable(void);
void hal_1v8_disable(void);

void hal_boost_5v_enable(void);
void hal_boost_5v_disable(void);
bool hal_boost_5v_is_enabled(void);
void hal_tr_switch_set(bool value);
void hal_pogo_in_set(bool value);
void hal_ship_control_set(bool value);
void hal_rpd_enable(void);
void hal_rpd_disable(void);
void hal_pdet_en_set(bool enable);
void hal_5353_key_set(bool pressed);

void hal_5353_key_release(void);
void hal_5353_key_rearm(void);

bool hal_key_pressed(void);
bool hal_key_get(void);
bool hal_hall_get(void);

void hal_hall_pull_sync(void);
bool hal_bat_int_get(void);
bool hal_charger_int_get(void);
bool hal_nint_get(void);
bool hal_pdetb_get(void);

#ifdef HIL_TEST

void hal_hall_set_mock(bool open);
#endif

#endif
