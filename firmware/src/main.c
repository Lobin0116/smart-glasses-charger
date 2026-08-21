#include "gd32e23x.h"

#include <stddef.h>

#include "aux_logic.h"
#include "button.h"
#include "charge_flow.h"
#include "cw2017.h"
#include "hal_exti.h"
#include "hal_wwdgt.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "hal_usart.h"
#include "ip5353.h"
#include "led.h"
#include "led_effect.h"
#include "mt5706.h"
#include "power_mgmt.h"
#include "state_machine.h"
#ifdef HIL_TEST
    #include "update_mode.h"
#endif

#define SOC_REFRESH_MS   500U   /* IP5353/CW2017 status poll interval.
                                 * 500 ms balances responsiveness (charge full,
                                 * NTC trip, USB unplug → sleep all show up
                                 * within half a second) against I2C traffic
                                 * (~1 ms per read, negligible vs Deep-Sleep
                                 * savings). CHAGER_INT EXTI is the primary
                                 * trigger; this is the safety-net poll. */
#define EXTI_DEBOUNCE_MS 20U    /* KEY only — HALL is polled by sm_tick. */

led_effect_ctx_t g_led_ctx;

#ifdef HIL_TEST
sm_ctx_t sm;
#else
static sm_ctx_t sm;
#endif
static uint32_t last_soc_refresh;
/* KEY still uses an EXTI event queue (the button needs debounce, and the
 * EXTI→button_on_press path was already working). HALL no longer uses this
 * queue — sm_tick polls hal_hall_get() directly at the top of each call. The
 * other EXTI sources (CHARGER/BAT/COIL) only need to wake the main loop so
 * refresh_case_status runs; exti_woken covers that and their pending bits
 * are cleared without any handler. */
static volatile uint8_t exti_pending;
static volatile uint32_t exti_last_trigger_ms[16];
static volatile bool exti_woken;

static void exti_callback(uint8_t line)
{
    if (line < 16U) {
        /* Only KEY needs the pending-bit + debounce path. HALL is sampled by
         * sm_tick on every call, so its edges don't need to be queued (and
         * queuing them caused the "single motion lost, repeated motion seen"
         * bug, because handshake blocking collapsed multiple edges into one
         * queue bit). CHARGER/BAT/COIL just need exti_woken. */
        if (line == HAL_EXTI_LINE_KEY) {
            uint8_t mask = (uint8_t)(1U << line);
            if ((exti_pending & mask) == 0U) {
                exti_last_trigger_ms[line] = hal_timer_get_ms();
            }
            exti_pending |= mask;
        } else if (line == HAL_EXTI_LINE_HALL) {
            /* Tag the edge so sm_tick re-runs the lid path even if the level
             * ended up where it started (close+open inside one handshake
             * burst — pure level polling would miss it). */
            sm.hall_edge_seen = true;
        }
        exti_woken = true;
    }
}

static void process_exti_events(void)
{
    if (exti_pending == 0U) {
        return;
    }
    uint32_t now = hal_timer_get_ms();

    if ((exti_pending & (1U << HAL_EXTI_LINE_KEY))
        && (now - exti_last_trigger_ms[HAL_EXTI_LINE_KEY] >= EXTI_DEBOUNCE_MS)) {
        exti_pending &= (uint8_t)~(1U << HAL_EXTI_LINE_KEY);
        button_on_press();
    }

    /* CHARGER_INT / BAT_INT / COIL_INT only needed to wake the loop; their
     * bits (if ever set, which is rare on this board) are dropped here. */
    exti_pending &= (uint8_t)~((1U << HAL_EXTI_LINE_CHARGER_INT)
                               | (1U << HAL_EXTI_LINE_BAT_INT)
                               | (1U << HAL_EXTI_LINE_COIL_INT));
}

static void refresh_case_status(void)
{
    uint8_t soc = cw2017_get_soc();
    sm.case_soc = soc;
    sm.ntc_temp_c = cw2017_get_temp_c();

    bool charging = ip5353_is_charging();
    bool input_valid = ip5353_is_input_valid();
    bool full = ip5353_is_full();

    charge_arbitrate(input_valid, mt5706_has_event());

    led_effect_set_case_info(&g_led_ctx, soc, charging || input_valid, full);
}

void board_init(void)
{
    hal_gpio_init();
    hal_timer_init();
    hal_i2c_init();
    hal_usart_init();
    hal_exti_init();
    hal_exti_register_callback(exti_callback);
    hal_wwdgt_init(20);
    led_init();
    cw2017_init();
}

int main(void)
{
    board_init();
    sm_init(&sm);
    led_effect_init(&g_led_ctx);
    button_init();
    hal_pwr_idle();

    /* Drop spurious EXTI edges captured during power-rail settling. */
    exti_pending = 0U;
#ifdef HIL_TEST
    /* HIL tests drive lid state via OPEN/CLOSE commands (hal_hall_set_mock),
     * so disable the physical HALL EXTI — the test PC will set the level. */
    exti_interrupt_disable(EXTI_4);
    exti_interrupt_flag_clear(EXTI_4);
#endif

    /* CW2017 SOC engine needs settling time after the quickstart triggered in
     * cw2017_init(); reading it immediately returns a transitional 0, which
     * would set sm.case_soc=0 and force the low-SOC path (MAINTAINING instead
     * of CHARGING on next handshake). */
    hal_timer_delay_ms(500);

    refresh_case_status();
    last_soc_refresh = hal_timer_get_ms();

    hal_wwdgt_feed();

    while (1) {
        /* EXTI wake-up: re-read charge/SOC state immediately so the state
         * machine sees the new world before deciding whether to sleep again.
         * Without this, a USB-plug wake would see the stale pre-sleep
         * case_charging=false and go right back to Deep-Sleep. */
        if (exti_woken) {
            exti_woken = false;
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }
        process_exti_events();
#ifdef HIL_TEST
        /* update_mode_poll runs before sm_tick so injected commands (OPEN/CLOSE/
         * KEY/RESET/OTA) are read before sm_do_*_heartbeat consumes RX bytes
         * looking for a heartbeat response. */
        update_mode_poll();
#endif
        sm_tick(&sm);
        button_poll();
        led_effect_poll(&g_led_ctx);

        if (hal_timer_expired(last_soc_refresh, SOC_REFRESH_MS)) {
            refresh_case_status();
            button_set_case_soc(sm.case_soc);
            last_soc_refresh = hal_timer_get_ms();
        }

        hal_wwdgt_feed();

#ifndef HIL_TEST
        /* Gate sleep on exti_pending so a KEY edge that fired this iteration
         * (20 ms debounce not yet elapsed) is not lost. HALL doesn't need
         * this gate — sm_tick re-samples hal_hall_get() on every wake. */
        if (sm_can_sleep(&sm) && exti_pending == 0U) {
            pm_enter_deep_sleep();
        }
#endif
    }
}
