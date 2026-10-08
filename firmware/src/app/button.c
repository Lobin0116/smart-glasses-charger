#include "button.h"

#include "cw2017.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "led_effect.h"

extern led_effect_ctx_t g_led_ctx;

static uint8_t btn_case_soc;

void button_set_case_soc(uint8_t soc) { btn_case_soc = soc; }

typedef enum
{
    BTN_IDLE,
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
                /* Event-triggered press (bench lesson 2026-10-08): the KEY
                 * falling edge is latched by the EXTI ISR and already
                 * debounced 20 ms on the EXTI side before button_on_press
                 * runs — do NOT re-check the pin level here. A level re-check
                 * scheduled by a slow loop (the ~1.1 s blocking handshake
                 * rounds) lands seconds after a normal short press has been
                 * released, discarding it — the "dead button for the whole
                 * handshake window" symptom. Trust the edge, light the
                 * display now; it carries its own 7 s expiry. */
                btn_raw_pressed = false;
                btn_press_ms = now;
                btn_state = BTN_PRESSED;
                led_effect_show_battery(&g_led_ctx, btn_case_soc);
            }
            break;

        case BTN_PRESSED:
            if (!hal_key_pressed()) {
                btn_state = BTN_IDLE;
            }
            break;
    }
}
