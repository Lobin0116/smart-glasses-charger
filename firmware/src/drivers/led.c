#include <stdint.h>

#include "hal_gpio.h"
#include "hal_timer.h"
#include "led.h"

/* Software PWM breath driven by TIMER13 at 10 kHz (0.1 ms tick). The 10 ms
 * carrier (100 Hz, well above flicker) is sliced into 100 sub-ticks, giving
 * 1 % duty resolution — fine enough that the breath curve looks continuous
 * instead of stepping through 4-5 visible brightness levels.
 *
 * Previous implementation ran PWM from the 1 kHz SysTick, which capped duty
 * at 10 levels (10 ms / 1 ms). With TIMER13 dedicated to LED PWM we get
 * 10× finer resolution without disturbing the millisecond time base. */
#define LED_PWM_PERIOD_SUBTICKS 100U  /* 10 ms carrier @ 0.1 ms/tick = 100 Hz */
#define LED_BREATH_PERIOD_MS    2500U
#define LED_BLINK_PERIOD_MS     1000U

/* Universal brightness ceiling for every LED mode (ON/BLINK/BREATH). Cap
 * matches the breath peak so nothing ever looks brighter than the breath's
 * brightest moment — consistent perceived brightness across the UI. */
#define LED_MAX_DUTY_PCT 30U

/* Breath sweeps 0..MAX (gamma-corrected parabola). */
#define LED_BREATH_DUTY_MIN 0U
#define LED_BREATH_DUTY_MAX LED_MAX_DUTY_PCT

/* Solid LED_ON: hold the max duty. */
#define LED_ON_DUTY_PCT LED_MAX_DUTY_PCT

/* Blink: PWM at max duty during the on half, fully off during the off half. */
#define LED_BLINK_DUTY_PCT LED_MAX_DUTY_PCT

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

/* Gamma-2.0 corrected parabolic envelope over one breath period.
 * Returns duty in [MIN..MAX] (percent units, 0..100).
 *
 *   parabola = 4 * x * (1 - x),  x in [0, 1]   →  bell curve 0→1→0
 *   gamma    = parabola^2                       →  expands dim end
 *
 * Sampling (period 2500 ms, MAX=30):
 *   phase  x     parabola  gamma   duty
 *   0      0     0         0        0   (trough, LED off)
 *   312    0.125 0.44      0.19     5
 *   625    0.25  0.75      0.56    16
 *   937    0.375 0.94      0.88    26
 *   1250   0.5   1.00      1.00    30   (peak)
 *   1562   0.625 0.94      0.88    26
 *   1875   0.75  0.75      0.56    16
 *   2187   0.875 0.44      0.19     5
 *   2500   1.0   0         0        0   (trough)
 *
 * With 100-level duty, adjacent samples differ by ≤ 11 sub-ticks (0.11 ms
 * on-time delta at 100 Hz), which is below the eye's step-fusion threshold. */
static uint32_t led_breath_duty(uint32_t phase_ms)
{
    uint32_t x = (phase_ms * 100U) / LED_BREATH_PERIOD_MS;  /* 0..100 */
    uint32_t parabola = (4U * x * (100U - x)) / 100U;       /* 0..100, bell */
    uint32_t gamma = (parabola * parabola) / 100U;          /* 0..100, dim-expanded */
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
    /* LED_OFF is written immediately so the pin goes dark without waiting
     * for the next TIMER13 tick. LED_ON/BREATH/BLINK are all driven by
     * led_pwm_tick in the TIMER13 ISR (LED_ON at a fixed PWM duty, no
     * longer a hard GPIO high) — first tick is within 0.1 ms, invisible. */
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
        color = LED_RED; /* <15%: red, covering the 5-15% and <5% bands */
    }
    led_set(color, LED_ON);
}

void led_poll(void)
{
    /* BREATH/BLINK PWM is driven from TIMER13 @ 10 kHz (led_pwm_tick), not
     * the main loop. This function is a no-op retained because led_effect_poll
     * calls it. Static LED_OFF/LED_ON states are written once by
     * led_set/led_all_off. */
}

/* Called from TIMER13_IRQHandler at 10 kHz (0.1 ms cadence). Each call
 * advances the PWM sub-tick (0..99) and re-applies BREATH/BLINK LEDs against
 * the new phase. Cost is ~50 cycles/tick (one integer divide per active LED
 * inside led_breath_duty), ~0.7 % of the 72 MHz budget at full load.
 *
 * Race with main-loop led_set: mode/phase_start are word-sized atomic
 * writes, and a 0.1 ms window of stale state produces at most one wrong
 * brightness step — invisible. */
void led_pwm_tick(void)
{
    static uint32_t pwm_sub = 0U;  /* 0..99, sub-ms PWM carrier phase */
    pwm_sub = (pwm_sub + 1U) % LED_PWM_PERIOD_SUBTICKS;

    uint32_t now_ms = hal_timer_get_ms();

    for (uint32_t i = 0U; i < LED_COLOR_COUNT; i++) {
        led_state_t *state = &leds[i];
        led_mode_t mode = state->mode;
        if (mode == LED_OFF) {
            continue;
        }
        if (mode == LED_ON) {
            /* Solid-on at a throttled PWM duty (LED_ON_DUTY_PCT) so it
             * matches the breath brightness scale instead of slamming the
             * pin to VDD for a 100 % on that's blinding in a dark room. */
            led_apply(state, (led_color_t)i, pwm_sub < LED_ON_DUTY_PCT);
            continue;
        }
        if (mode == LED_BREATH) {
            uint32_t phase = (now_ms - state->phase_start) % LED_BREATH_PERIOD_MS;
            uint32_t duty = led_breath_duty(phase);  /* 0..MAX (percent) */
            led_apply(state, (led_color_t)i, pwm_sub < duty);
        } else { /* LED_BLINK — PWM at LED_BLINK_DUTY_PCT during on-half, off otherwise */
            uint32_t phase = (now_ms - state->phase_start) % LED_BLINK_PERIOD_MS;
            bool in_on_half = phase < (LED_BLINK_PERIOD_MS / 2U);
            led_apply(state, (led_color_t)i, in_on_half && (pwm_sub < LED_BLINK_DUTY_PCT));
        }
    }
}
