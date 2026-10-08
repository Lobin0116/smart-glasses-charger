#include "button.h"

#include "cw2017.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "led_effect.h"

extern led_effect_ctx_t g_led_ctx;

static uint8_t btn_case_soc;

void button_set_case_soc(uint8_t soc) { btn_case_soc = soc; }

#define DEBOUNCE_MS 50U

typedef enum
{
    BTN_IDLE,
    BTN_DEBOUNCE,
    BTN_PRESSED,
} btn_state_t;

static btn_state_t btn_state;
static uint32_t btn_press_ms;
static volatile bool btn_raw_pressed;

void button_init(void)
{
    btn_state = BTN_IDLE;
    btn_raw_pressed = false;
}

void button_on_press(void) { btn_raw_pressed = true; }

bool button_is_busy(void)
{
    return btn_state != BTN_IDLE;
}

void button_poll(void)
{
    uint32_t now = hal_timer_get_ms();

    switch (btn_state) {
        case BTN_IDLE:
            if (btn_raw_pressed) {
                btn_raw_pressed = false;
                btn_press_ms = now;
                btn_state = BTN_DEBOUNCE;
            }
            break;

        case BTN_DEBOUNCE:
            if (hal_timer_expired(btn_press_ms, DEBOUNCE_MS)) {
                if (hal_key_pressed()) {
                    /* Press-triggered feedback (user decision 2026-10-08):
                     * light the battery display the moment the press is
                     * confirmed instead of waiting for release. The old
                     * release-triggered design (plus the >=2 s hold gate)
                     * made a press vanish entirely whenever the main loop
                     * was slow to poll — e.g. during the ~1.7 s blocking
                     * handshake cadence, which read as a dead button. The
                     * display carries its own 7 s expiry. */
                    btn_state = BTN_PRESSED;
                    led_effect_show_battery(&g_led_ctx, btn_case_soc);
                } else {
                    btn_state = BTN_IDLE;
                }
            }
            break;

        case BTN_PRESSED:
            if (!hal_key_pressed()) {
                btn_state = BTN_IDLE;
            }
            break;
    }
}
