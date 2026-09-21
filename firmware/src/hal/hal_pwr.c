#include "hal_pwr.h"

#include "hal_gpio.h"
#include "hal_timer.h"

#define HAL_PWR_DISCHARGE_MS 100U

typedef enum
{
    HAL_PWR_IDLE,
    HAL_PWR_CHARGE,
    HAL_PWR_COMM,
} hal_pwr_state_t;

static hal_pwr_state_t pwr_state = HAL_PWR_IDLE;

void hal_pwr_idle(void)
{

    hal_pogo_in_set(false);
    hal_rpd_disable();
    pwr_state = HAL_PWR_IDLE;
}

void hal_pwr_enter_charge(void)
{

    hal_pogo_in_set(false);
    hal_rpd_disable();
    pwr_state = HAL_PWR_CHARGE;
}

void hal_pwr_enter_comm(void)
{

    hal_rpd_enable();
    hal_timer_delay_ms(HAL_PWR_DISCHARGE_MS);
    hal_rpd_disable();

    hal_1v8_enable();
    hal_pogo_in_set(true);
    pwr_state = HAL_PWR_COMM;
}

void hal_pwr_discharge(uint32_t ms)
{
    hal_rpd_enable();
    hal_timer_delay_ms(ms);
    hal_rpd_disable();
}

static void hal_pwr_restore(hal_pwr_state_t state)
{
    switch (state) {
        case HAL_PWR_COMM:

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

void hal_pwr_pulse_charge(uint32_t ms)
{
    hal_pwr_state_t prev = pwr_state;
    hal_pwr_enter_charge();
    hal_timer_delay_ms(ms);
    hal_pwr_restore(prev);
}
