#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>

#include "hal_pinmux.h"

/* Configure every board GPIO per the CONTEXT.md pin table: enable the port
 * clocks, set direction / pull / alternate function, and drive all outputs
 * low. Must run before any peripheral that depends on these pins. */
void hal_gpio_init(void);

/* Generic helpers operating on a logical pin from hal_pin_t. */
void hal_gpio_set(hal_pin_t pin, bool value);
bool hal_gpio_get(hal_pin_t pin);
void hal_gpio_toggle(hal_pin_t pin);

/* LEDs are active high. */
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
void hal_led_2812_on(void);
void hal_led_2812_off(void);

/* Power and control outputs. */
void hal_1v8_enable(void);
void hal_1v8_disable(void);
void hal_chip_en2_enable(void);
void hal_chip_en2_disable(void);
void hal_tr_switch_set(bool value);
void hal_pogo_in_set(bool value);
void hal_ship_control_set(bool value);
void hal_rpd_enable(void);
void hal_rpd_disable(void);

/* Input reads (return the live pad level). */
bool hal_key_pressed(void);
bool hal_key_get(void);
bool hal_hall_get(void);
bool hal_bat_int_get(void);
bool hal_charger_int_get(void);
bool hal_coil_int_get(void);

#endif /* HAL_GPIO_H */
