#include "aux_logic.h"

#include "charge_flow.h"
#include "hal_gpio.h"
#include "led.h"

/* --- Task 25: NTC temperature protection --- */

ntc_zone_t ntc_get_zone(int8_t temp_c)
{
    if (temp_c < 0) {
        return NTC_COLD;
    }
    if (temp_c < 15) {
        return NTC_COLD;
    }
    if (temp_c < 45) {
        return NTC_MILD;
    }
    if (temp_c <= 60) {
        return NTC_WARM;
    }
    return NTC_CRITICAL;
}

bool ntc_should_reduce_charge(ntc_zone_t zone) { return zone == NTC_COLD || zone == NTC_WARM; }

bool ntc_should_stop_charge(ntc_zone_t zone) { return zone == NTC_CRITICAL; }

/* Wired/wireless arbitration (old Task 26) was deleted with the V1 board —
 * see aux_logic.h for the V2 rationale. */

/* --- Task 27: Recharge logic --- */

bool recharge_check(uint8_t glass_soc, bool glass_full)
{
    if (glass_full) {
        return false;
    }
    return glass_soc < RECHARGE_THRESHOLD;
}

/* --- Task 28+30: Lid event helpers --- */

bool lid_check_glass_present(sm_ctx_t *ctx) { return ctx->glass_present; }

void lid_no_glass_display(uint8_t case_soc)
{
    led_all_off();
    led_color_t color = LED_WHITE;
    if (case_soc <= 5U) {
        led_set(LED_RED, LED_BLINK);
        return;
    }
    if (case_soc < 15U) {
        color = LED_RED;
    } else if (case_soc <= 40U) {
        color = LED_GREEN;
    }
    led_set(color, LED_ON);
}
