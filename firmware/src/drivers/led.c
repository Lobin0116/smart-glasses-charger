#include <stdint.h>

#include "hal_gpio.h"
#include "hal_timer.h"
#include "led.h"

/* Breath is a software PWM: a 20 ms carrier (50 Hz, flicker-free) sliced into
 * LED_PWM_STEPS duty levels, with the duty ramped over a triangle so the
 * perceived brightness glows min->max->min. Blink is a plain 1 Hz toggle.
 * Breath period is 1.5 s — short enough that the trough doesn't linger
 * (smoother perceived fade) but long enough to read as a breath, not a pulse. */
#define LED_PWM_PERIOD_MS    20U
#define LED_PWM_STEPS        20U
#define LED_BREATH_PERIOD_MS 1500U
#define LED_BLINK_PERIOD_MS  1000U

/* Breath duty is clamped to [MIN, MAX] instead of [0, LED_PWM_STEPS]:
 *  - MIN keeps the LED visibly lit at the trough so the cycle never looks
 *    "off → suddenly on" (human eye can't see the bottom of a 0→N ramp, the
 *    fade appears to stutter).
 *  - MAX caps peak brightness so the cycle doesn't feel harsh at the top.
 * Tuned quite dim: 10% trough, 50% peak — subtle glow, won't light up a
 * dark room. */
#define LED_BREATH_DUTY_MIN 2U   /* 2/20 = 10% — dim but visible at trough */
#define LED_BREATH_DUTY_MAX 10U  /* 10/20 = 50% — soft peak */

typedef struct
{
    led_mode_t mode;
    uint32_t phase_start; /* hal_timer_get_ms() captured when the mode was set */
    bool last_on;         /* last physical output, to skip redundant writes */
} led_state_t;

static led_state_t leds[LED_COLOR_COUNT];

/* Today a 4-GPIO LED bank. A WS2812 backend on PB2 would replace only this
 * body with a single-pixel frame write; the rest of the driver is unchanged,
 * though that backend would throttle refreshes to changes itself. */
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

/* Write through to the pin only when the level changes. */
static void led_apply(led_state_t *state, led_color_t color, bool on)
{
    if (on != state->last_on) {
        state->last_on = on;
        led_hw_write(color, on);
    }
}

/* Triangle wave mapping one breath period to a LED_BREATH_DUTY_MIN..MAX duty.
 * The triangle goes 0→STEPS→0 in one period; we then linearly map that into
 * the [MIN, MAX] window so the LED stays visibly lit at the trough. */
static uint32_t led_breath_duty(uint32_t phase_ms)
{
    uint32_t half = LED_BREATH_PERIOD_MS / 2U;
    uint32_t progress;
    if (phase_ms < half) {
        progress = (phase_ms * LED_PWM_STEPS) / half;
    } else {
        progress = ((LED_BREATH_PERIOD_MS - phase_ms) * LED_PWM_STEPS) / half;
    }
    return LED_BREATH_DUTY_MIN
           + (progress * (LED_BREATH_DUTY_MAX - LED_BREATH_DUTY_MIN)) / LED_PWM_STEPS;
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
    /* Solid modes drive the pin now; blink and breath start on the next poll. */
    if (mode == LED_OFF) {
        led_apply(state, color, false);
    } else if (mode == LED_ON) {
        led_apply(state, color, true);
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
        color = LED_RED; /* <15%: red, covering the 5-15% and <5% bands */
    }
    led_set(color, LED_ON);
}

void led_poll(void)
{
    uint32_t now = hal_timer_get_ms();

    for (uint32_t i = 0; i < LED_COLOR_COUNT; i++) {
        led_state_t *state = &leds[i];
        led_color_t color = (led_color_t)i;

        switch (state->mode) {
            case LED_OFF:
                led_apply(state, color, false);
                break;
            case LED_ON:
                led_apply(state, color, true);
                break;
            case LED_BLINK: {
                uint32_t phase = (now - state->phase_start) % LED_BLINK_PERIOD_MS;
                led_apply(state, color, phase < (LED_BLINK_PERIOD_MS / 2U));
                break;
            }
            case LED_BREATH: {
                uint32_t phase = (now - state->phase_start) % LED_BREATH_PERIOD_MS;
                uint32_t duty = led_breath_duty(phase);
                led_apply(state, color, (now % LED_PWM_PERIOD_MS) < duty);
                break;
            }
        }
    }
}
