#include "power_mgmt.h"

#include "gd32e23x.h"
#include "hal_gpio.h"

void pm_enter_deep_sleep(void) {
    rcu_periph_clock_enable(RCU_PMU);

    /* Clear any pending EXTI flags before sleeping, so a stale edge does not
     * bounce us right back out of WFI before the real wake event arrives. */
    exti_interrupt_flag_clear(EXTI_4);
    exti_interrupt_flag_clear(EXTI_8);
    exti_interrupt_flag_clear(EXTI_11);
    exti_interrupt_flag_clear(EXTI_12);
    exti_interrupt_flag_clear(EXTI_3);

    pmu_to_deepsleepmode(PMU_LDO_NORMAL, WFI_CMD);

    /* After wake-up the core clock may have switched back to the internal 8 MHz
     * IRC. The caller (main loop) continues normally; SystemCoreClock is still
     * valid because Deep-Sleep preserves SRAM and registers. */
}

void pm_enter_standby(void) {
    rcu_periph_clock_enable(RCU_PMU);
    hal_ship_control_set(true);
    pmu_to_standbymode();
}

void pm_enter_ship_mode(void) { pm_enter_standby(); }

bool pm_check_wakeup_reason(void) {
    rcu_periph_clock_enable(RCU_PMU);
    if (RESET != pmu_flag_get(PMU_FLAG_WAKEUP)) {
        pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
        return true;
    }
    return false;
}
