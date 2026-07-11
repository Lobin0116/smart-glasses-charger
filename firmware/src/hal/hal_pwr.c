#include "hal_pwr.h"

#include "hal_gpio.h"
#include "hal_timer.h"

/* Minimum bus discharge before the UART path can be driven, per the timing spec
 * in CONTEXT.md. */
#define HAL_PWR_DISCHARGE_MS 100U

typedef enum {
    HAL_PWR_IDLE,   /* bus parked, no rail energised */
    HAL_PWR_CHARGE, /* POGO routed to the 5V charge rail */
    HAL_PWR_COMM,   /* POGO routed to the 1.8V UART path */
} hal_pwr_state_t;

static hal_pwr_state_t pwr_state = HAL_PWR_IDLE;

void hal_pwr_idle(void) {
    /* Drop the LDO first so the 1.8V rail never back-feeds while the switch
     * re-routes. RPD stays low so the bus is left high-impedance, not bled. */
    hal_1v8_disable();
    hal_pogo_in_set(false);
    hal_rpd_disable();
    pwr_state = HAL_PWR_IDLE;
}

void hal_pwr_enter_charge(void) {
    /* Same pin levels as idle; the difference is that the IP5353 boost is on,
     * which the IP5353 driver owns. The mode is tracked so a charge pulse can
     * restore it afterwards. */
    hal_1v8_disable();
    hal_pogo_in_set(false);
    hal_rpd_disable();
    pwr_state = HAL_PWR_CHARGE;
}

void hal_pwr_enter_comm(void) {
    /* Bleed the residual 5V off the POGO bus before the UART path is connected,
     * or the 1.8V transceiver fights a charged line. The timing spec floors this
     * discharge at 100ms; RPD releases again so it does not clamp the UART pad. */
    hal_rpd_enable();
    hal_timer_delay_ms(HAL_PWR_DISCHARGE_MS);
    hal_rpd_disable();

    /* Energise the 1.8V LDO, then steer the switch. IN rises last so the UART
     * pad is driven only after the rail has settled. */
    hal_1v8_enable();
    hal_pogo_in_set(true);
    pwr_state = HAL_PWR_COMM;
}

void hal_pwr_discharge(uint32_t ms) {
    hal_rpd_enable();
    hal_timer_delay_ms(ms);
    hal_rpd_disable();
}

static void hal_pwr_restore(hal_pwr_state_t state) {
    switch (state) {
    case HAL_PWR_COMM:
        /* Re-applying 5V during a pulse recharges the bus, so the full discharge
         * sequence must run again before UART is safe. */
        hal_pwr_enter_comm();
        break;
    case HAL_PWR_CHARGE:
        hal_pwr_enter_charge();
        break;
    case HAL_PWR_IDLE:
    default:
        hal_pwr_idle();
        break;
    }
}

void hal_pwr_pulse_charge(uint32_t ms) {
    hal_pwr_state_t prev = pwr_state;
    hal_pwr_enter_charge();
    hal_timer_delay_ms(ms);
    hal_pwr_restore(prev);
}
