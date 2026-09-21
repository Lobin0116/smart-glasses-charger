#include "power_mgmt.h"

#include "gd32e23x.h"
#include "hal_gpio.h"
#include "hal_i2c.h"

extern void SystemInit(void);

void pm_enter_deep_sleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    exti_interrupt_flag_clear(EXTI_2);
    exti_interrupt_flag_clear(EXTI_3);
    exti_interrupt_flag_clear(EXTI_4);
    exti_interrupt_flag_clear(EXTI_7);
    exti_interrupt_flag_clear(EXTI_8);

    hal_power_gate_off(HAL_POWER_GATE_POGO3V3);

    hal_boost_5v_disable();

    hal_power_gate_off(HAL_POWER_GATE_BAT);

    hal_1v8_disable();

    hal_i2c_pins_sleep();
    hal_5353_key_release();

    hal_hall_pull_sync();
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);

    SystemInit();
    SystemCoreClockUpdate();

    hal_power_gate_on(HAL_POWER_GATE_POGO3V3);
    hal_boost_5v_enable();
    hal_power_gate_on(HAL_POWER_GATE_BAT);
    hal_1v8_enable();
    hal_i2c_pins_resume();
    hal_5353_key_rearm();
    hal_hall_pull_sync();
}

void pm_enter_standby(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    hal_ship_control_set(true);
    pmu_to_standbymode();
}

void pm_enter_ship_mode(void) { pm_enter_standby(); }

bool pm_check_wakeup_reason(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    if (RESET != pmu_flag_get(PMU_FLAG_WAKEUP)) {
        pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
        return true;
    }
    return false;
}
