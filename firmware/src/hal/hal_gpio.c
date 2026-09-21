#include "hal_gpio.h"

#ifdef HIL_TEST

static bool hall_mock_open = false;

void hal_hall_set_mock(bool open) { hall_mock_open = open; }
#endif

typedef struct
{
    uint32_t port;
    uint32_t pin;
} hal_pin_map_t;

static const hal_pin_map_t pin_map[HAL_PIN_COUNT] = {
    [HAL_PIN_LED_RED] = {HAL_LED_RED_PORT, HAL_LED_RED_PIN},
    [HAL_PIN_LED_GREEN] = {HAL_LED_GREEN_PORT, HAL_LED_GREEN_PIN},
    [HAL_PIN_LED_BLUE] = {HAL_LED_BLUE_PORT, HAL_LED_BLUE_PIN},
    [HAL_PIN_LED_WHITE] = {HAL_LED_WHITE_PORT, HAL_LED_WHITE_PIN},
    [HAL_PIN_EN_1V8] = {HAL_EN_1V8_PORT, HAL_EN_1V8_PIN},
    [HAL_PIN_TR_SWITCH] = {HAL_TR_SWITCH_PORT, HAL_TR_SWITCH_PIN},
    [HAL_PIN_POGO_IN] = {HAL_POGO_IN_PORT, HAL_POGO_IN_PIN},
    [HAL_PIN_SHIP_CTRL] = {HAL_SHIP_CTRL_PORT, HAL_SHIP_CTRL_PIN},
    [HAL_PIN_RPD] = {HAL_RPD_PORT, HAL_RPD_PIN},
    [HAL_PIN_PDET_EN] = {HAL_PDET_EN_PORT, HAL_PDET_EN_PIN},
    [HAL_PIN_KEY5353] = {HAL_KEY5353_PORT, HAL_KEY5353_PIN},
    [HAL_PIN_POGO3V3_EN] = {HAL_POGO3V3_EN_PORT, HAL_POGO3V3_EN_PIN},
    [HAL_PIN_BOOST_5V_EN] = {HAL_BOOST_5V_EN_PORT, HAL_BOOST_5V_EN_PIN},
    [HAL_PIN_BAT_INT] = {HAL_BAT_INT_PORT, HAL_BAT_INT_PIN},
    [HAL_PIN_NINT] = {HAL_NINT_PORT, HAL_NINT_PIN},
    [HAL_PIN_CHARGER_INT] = {HAL_CHARGER_INT_PORT, HAL_CHARGER_INT_PIN},
    [HAL_PIN_PDETB] = {HAL_PDETB_PORT, HAL_PDETB_PIN},
    [HAL_PIN_KEY] = {HAL_KEY_PORT, HAL_KEY_PIN},
    [HAL_PIN_HALL] = {HAL_HALL_PORT, HAL_HALL_PIN},
    [HAL_PIN_I2C_SCL] = {HAL_I2C_SCL_PORT, HAL_I2C_SCL_PIN},
    [HAL_PIN_I2C_SDA] = {HAL_I2C_SDA_PORT, HAL_I2C_SDA_PIN},
    [HAL_PIN_UART_TX] = {HAL_UART_TX_PORT, HAL_UART_TX_PIN},
    [HAL_PIN_UART_RX] = {HAL_UART_RX_PORT, HAL_UART_RX_PIN},
};

void hal_gpio_init(void)
{

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOF);

    gpio_bit_set(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_bit_set(GPIOF, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_6 | GPIO_PIN_7);

    gpio_mode_set(GPIOB,
                  GPIO_MODE_OUTPUT,
                  GPIO_PUPD_NONE,
                  GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_output_options_set(GPIOB,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_2MHZ,
                            GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
                                | GPIO_PIN_15);
    gpio_bit_reset(GPIOB,
                   GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    gpio_bit_set(GPIOA, GPIO_PIN_15);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_15);

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_13);

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_3 | GPIO_PIN_4);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_7 | GPIO_PIN_8);

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_0);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_2);

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_10MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_af_set(GPIOB, GPIO_AF_1, GPIO_PIN_6 | GPIO_PIN_7);

    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
}

void hal_gpio_set(hal_pin_t pin, bool value)
{
    if (pin < HAL_PIN_COUNT) {
        if (value) {
            gpio_bit_set(pin_map[pin].port, pin_map[pin].pin);
        } else {
            gpio_bit_reset(pin_map[pin].port, pin_map[pin].pin);
        }
    }
}

bool hal_gpio_get(hal_pin_t pin)
{
    if (pin < HAL_PIN_COUNT) {
        return gpio_input_bit_get(pin_map[pin].port, pin_map[pin].pin) == SET;
    }
    return false;
}

void hal_gpio_toggle(hal_pin_t pin)
{
    if (pin < HAL_PIN_COUNT) {
        gpio_bit_toggle(pin_map[pin].port, pin_map[pin].pin);
    }
}

void hal_led_red_on(void) { hal_gpio_set(HAL_PIN_LED_RED, false); }

void hal_led_red_off(void) { hal_gpio_set(HAL_PIN_LED_RED, true); }

void hal_led_red_toggle(void) { hal_gpio_toggle(HAL_PIN_LED_RED); }

void hal_led_green_on(void) { hal_gpio_set(HAL_PIN_LED_GREEN, false); }

void hal_led_green_off(void) { hal_gpio_set(HAL_PIN_LED_GREEN, true); }

void hal_led_green_toggle(void) { hal_gpio_toggle(HAL_PIN_LED_GREEN); }

void hal_led_blue_on(void) { hal_gpio_set(HAL_PIN_LED_BLUE, false); }

void hal_led_blue_off(void) { hal_gpio_set(HAL_PIN_LED_BLUE, true); }

void hal_led_blue_toggle(void) { hal_gpio_toggle(HAL_PIN_LED_BLUE); }

void hal_led_white_on(void) { hal_gpio_set(HAL_PIN_LED_WHITE, false); }

void hal_led_white_off(void) { hal_gpio_set(HAL_PIN_LED_WHITE, true); }

void hal_led_white_toggle(void) { hal_gpio_toggle(HAL_PIN_LED_WHITE); }

static const hal_pin_map_t gate_map[HAL_POWER_GATE_COUNT] = {
    [HAL_POWER_GATE_POGO3V3] = {HAL_POGO3V3_EN_PORT, HAL_POGO3V3_EN_PIN},
    [HAL_POWER_GATE_BAT] = {HAL_BAT_PMOS_EN_PORT, HAL_BAT_PMOS_EN_PIN},
};

void hal_power_gate_on(hal_power_gate_t gate)
{
    if (gate >= HAL_POWER_GATE_COUNT) {
        return;
    }
    const hal_pin_map_t *p = &gate_map[gate];
    gpio_bit_reset(p->port, p->pin);
    gpio_mode_set(p->port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, p->pin);
    gpio_output_options_set(p->port, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, p->pin);
}

void hal_power_gate_off(hal_power_gate_t gate)
{
    if (gate >= HAL_POWER_GATE_COUNT) {
        return;
    }
    const hal_pin_map_t *p = &gate_map[gate];
    gpio_mode_set(p->port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, p->pin);
}

bool hal_power_gate_is_on(hal_power_gate_t gate)
{
    if (gate >= HAL_POWER_GATE_COUNT) {
        return false;
    }

    return gpio_input_bit_get(gate_map[gate].port, gate_map[gate].pin) == RESET;
}

void hal_1v8_enable(void) { hal_gpio_set(HAL_PIN_EN_1V8, true); }

void hal_1v8_disable(void) { hal_gpio_set(HAL_PIN_EN_1V8, false); }

void hal_boost_5v_enable(void) { hal_gpio_set(HAL_PIN_BOOST_5V_EN, true); }

void hal_boost_5v_disable(void) { hal_gpio_set(HAL_PIN_BOOST_5V_EN, false); }

bool hal_boost_5v_is_enabled(void) { return hal_gpio_get(HAL_PIN_BOOST_5V_EN); }

void hal_tr_switch_set(bool value) { hal_gpio_set(HAL_PIN_TR_SWITCH, value); }

void hal_pogo_in_set(bool value) { hal_gpio_set(HAL_PIN_POGO_IN, value); }

void hal_ship_control_set(bool value) { hal_gpio_set(HAL_PIN_SHIP_CTRL, value); }

void hal_rpd_enable(void) { hal_gpio_set(HAL_PIN_RPD, true); }

void hal_rpd_disable(void) { hal_gpio_set(HAL_PIN_RPD, false); }

void hal_pdet_en_set(bool enable) { hal_gpio_set(HAL_PIN_PDET_EN, enable); }

void hal_5353_key_set(bool pressed) { hal_gpio_set(HAL_PIN_KEY5353, !pressed); }

void hal_5353_key_release(void)
{

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, HAL_KEY5353_PIN);
}

void hal_5353_key_rearm(void)
{

    gpio_bit_set(GPIOA, HAL_KEY5353_PIN);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HAL_KEY5353_PIN);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, HAL_KEY5353_PIN);
}

bool hal_key_pressed(void)
{

    return !hal_gpio_get(HAL_PIN_KEY);
}

bool hal_key_get(void) { return hal_gpio_get(HAL_PIN_KEY); }

bool hal_hall_get(void)
{
#ifdef HIL_TEST
    return hall_mock_open;
#else
    return hal_gpio_get(HAL_PIN_HALL);
#endif
}

void hal_hall_pull_sync(void)
{
#ifndef HIL_TEST

    const uint32_t pupd = gpio_input_bit_get(HAL_HALL_PORT, HAL_HALL_PIN) ? GPIO_PUPD_PULLUP : GPIO_PUPD_PULLDOWN;
    gpio_mode_set(HAL_HALL_PORT, GPIO_MODE_INPUT, pupd, HAL_HALL_PIN);
#endif
}

bool hal_bat_int_get(void) { return hal_gpio_get(HAL_PIN_BAT_INT); }

bool hal_charger_int_get(void) { return hal_gpio_get(HAL_PIN_CHARGER_INT); }

bool hal_nint_get(void) { return hal_gpio_get(HAL_PIN_NINT); }

bool hal_pdetb_get(void) { return hal_gpio_get(HAL_PIN_PDETB); }
