#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>

#include "hal_pinmux.h"

/* Configure every board GPIO per the Board1_V2 pin table (hal_pinmux.h):
 * enable the port clocks, set direction / pull / alternate function, and
 * drive all outputs to their idle level. Must run before any peripheral that
 * depends on these pins. */
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

/* --- Module power gates (V2) -------------------------------------------- */
/* Every board power switch is the same circuit: PMOS high-side, gate pulled
 * up to 3V3 by 10k, MCU pin on the gate. "On" = drive LOW; "off" = release
 * the pad to high-impedance so the pull-up holds the PMOS cut (never drive
 * high — the pad must float when asleep). The future Q4 battery-PMOS fly
 * wire will join this enum unchanged. */
typedef enum
{
    HAL_POWER_GATE_POGO3V3,  /* PB11: ET3328 + BL1551B 3V3 side */
    HAL_POWER_GATE_UART3V3,  /* PB5: CH340K supply (unused this round, stays off) */
    HAL_POWER_GATE_COUNT
} hal_power_gate_t;

void hal_power_gate_on(hal_power_gate_t gate);
void hal_power_gate_off(hal_power_gate_t gate);
bool hal_power_gate_is_on(hal_power_gate_t gate);

/* Power and control outputs. */
void hal_1v8_enable(void);
void hal_1v8_disable(void);
void hal_tr_switch_set(bool value);
void hal_pogo_in_set(bool value);
void hal_ship_control_set(bool value);
void hal_rpd_enable(void);
void hal_rpd_disable(void);
void hal_pdet_en_set(bool enable);
void hal_5353_key_set(bool pressed);

/* Input reads (return the live pad level). */
bool hal_key_pressed(void);
bool hal_key_get(void);
bool hal_hall_get(void);
bool hal_bat_int_get(void);
bool hal_charger_int_get(void);
bool hal_nint_get(void);
bool hal_pdetb_get(void);

#ifdef HIL_TEST
/* Test-only: override the HALL level that hal_hall_get reports, so HIL tests
 * can drive lid transitions over UART without a physical magnet. The mock is
 * only consulted under HIL_TEST; the production build reads the real pad. */
void hal_hall_set_mock(bool open);
#endif

#endif /* HAL_GPIO_H */
