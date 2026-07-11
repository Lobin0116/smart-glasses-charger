#include "button.h"

#include "cw2017.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "led_effect.h"

extern led_effect_ctx_t g_led_ctx;

static uint8_t btn_case_soc;

void button_set_case_soc(uint8_t soc) { btn_case_soc = soc; }

#define DEBOUNCE_MS 50U
#define SHORT_PRESS_MS 2000U

typedef enum {
    BTN_IDLE,
    BTN_DEBOUNCE,
    BTN_PRESSED,
} btn_state_t;

static btn_state_t btn_state;
static uint32_t btn_press_ms;
static volatile bool btn_raw_pressed;

void button_init(void) {
    btn_state = BTN_IDLE;
    btn_raw_pressed = false;
}

void button_on_press(void) { btn_raw_pressed = true; }

void button_poll(void) {
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
                btn_state = BTN_PRESSED;
            } else {
                btn_state = BTN_IDLE;
            }
        }
        break;

    case BTN_PRESSED:
        if (!hal_key_pressed()) {
            uint32_t held = hal_timer_elapsed(btn_press_ms);
            if (held < SHORT_PRESS_MS) {
                led_effect_show_battery(&g_led_ctx, btn_case_soc);
            }
            btn_state = BTN_IDLE;
        }
        break;
    }
}
