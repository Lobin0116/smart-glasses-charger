#include "power_mgmt.h"

#include "gd32e23x.h"
#include "hal_gpio.h"

void pm_enter_deep_sleep(void) {
    rcu_periph_clock_enable(RCU_PMU);
    pmu_to_deepsleepmode(PMU_LDO_NORMAL, WFI_CMD);
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
