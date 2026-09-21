#include <stdint.h>

#include "hal_gpio.h"
#include "hal_timer.h"
#include "led.h"
#define LED_PWM_PERIOD_SUBTICKS 100U
#define LED_BREATH_PERIOD_MS 2500U
#define LED_BLINK_PERIOD_MS 1000U

#define LED_MAX_DUTY_PCT 30U

#define LED_BREATH_DUTY_MIN 0U
#define LED_BREATH_DUTY_MAX LED_MAX_DUTY_PCT

#define LED_ON_DUTY_PCT LED_MAX_DUTY_PCT

#define LED_BLINK_DUTY_PCT LED_MAX_DUTY_PCT

typedef struct
{
    led_mode_t mode;
    uint32_t phase_start;
    bool last_on;
} led_state_t;

static led_state_t leds[LED_COLOR_COUNT];

static void led_hw_write(led_color_t color, bool on)
{
    switch (color) {
        case LED_RED:
            on ? hal_led_red_on() : hal_led_red_off();
            break;
        case LED_GREEN:
            on ? hal_led_green_on() : hal_led_green_off();
            break;
        case LED_BLUE:
            on ? hal_led_blue_on() : hal_led_blue_off();
            break;
        case LED_WHITE:
            on ? hal_led_white_on() : hal_led_white_off();
            break;
        default:
            break;
    }
}

static void led_apply(led_state_t *state, led_color_t color, bool on)
{
    if (on != state->last_on) {
        state->last_on = on;
        led_hw_write(color, on);
    }
}
static uint32_t led_breath_duty(uint32_t phase_ms)
{
    uint32_t x = (phase_ms * 100U) / LED_BREATH_PERIOD_MS;
    uint32_t parabola = (4U * x * (100U - x)) / 100U;
    uint32_t gamma = (parabola * parabola) / 100U;
    return LED_BREATH_DUTY_MIN
           + (gamma * (LED_BREATH_DUTY_MAX - LED_BREATH_DUTY_MIN)) / 100U;
}

void led_init(void)
{
    for (uint32_t i = 0; i < LED_COLOR_COUNT; i++) {
        leds[i].mode = LED_OFF;
        leds[i].phase_start = 0U;
        leds[i].last_on = false;
        led_hw_write((led_color_t)i, false);
    }
}

void led_set(led_color_t color, led_mode_t mode)
{
    if (color >= LED_COLOR_COUNT) {
        return;
    }
    led_state_t *state = &leds[color];
    state->mode = mode;
    state->phase_start = hal_timer_get_ms();

    if (mode == LED_OFF) {
        led_apply(state, color, false);
    }
}

void led_all_off(void)
{
    for (uint32_t i = 0; i < LED_COLOR_COUNT; i++) {
        leds[i].mode = LED_OFF;
        led_apply(&leds[i], (led_color_t)i, false);
    }
}

void led_set_by_soc(uint8_t soc)
{
    led_color_t color;
    if (soc > 40U) {
        color = LED_WHITE;
    } else if (soc >= 15U) {
        color = LED_GREEN;
    } else {
        color = LED_RED;
    }
    led_set(color, LED_ON);
}

void led_poll(void)
{

}
void led_pwm_tick(void)
{
    static uint32_t pwm_sub = 0U;
    pwm_sub = (pwm_sub + 1U) % LED_PWM_PERIOD_SUBTICKS;

    uint32_t now_ms = hal_timer_get_ms();

    for (uint32_t i = 0U; i < LED_COLOR_COUNT; i++) {
        led_state_t *state = &leds[i];
        led_mode_t mode = state->mode;
        if (mode == LED_OFF) {
            continue;
        }
        if (mode == LED_ON) {

            led_apply(state, (led_color_t)i, pwm_sub < LED_ON_DUTY_PCT);
            continue;
        }
        if (mode == LED_BREATH) {
            uint32_t phase = (now_ms - state->phase_start) % LED_BREATH_PERIOD_MS;
            uint32_t duty = led_breath_duty(phase);
            led_apply(state, (led_color_t)i, pwm_sub < duty);
        } else {
            uint32_t phase = (now_ms - state->phase_start) % LED_BLINK_PERIOD_MS;
            bool in_on_half = phase < (LED_BLINK_PERIOD_MS / 2U);
            led_apply(state, (led_color_t)i, in_on_half && (pwm_sub < LED_BLINK_DUTY_PCT));
        }
    }
}
