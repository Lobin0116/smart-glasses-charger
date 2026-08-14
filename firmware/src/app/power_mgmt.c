#include "power_mgmt.h"

#include "gd32e23x.h"
#include "hal_gpio.h"

/* Provided by the CMSIS startup file. Re-running it after a Deep-Sleep wake
 * is the documented way to restore the PLL: with PMU_LDO_LOWPOWER the part
 * drops to IRC8M while asleep and the PLL is off on wake, so every clock-
 * dependent peripheral (SysTick reload, USART baud, I2C timing) would be
 * wrong by the 8 MHz / 72 MHz ratio until we re-arm the PLL. */
extern void SystemInit(void);

void pm_enter_deep_sleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    /* Clear any pending EXTI flags before sleeping, so a stale edge does not
     * bounce us right back out of WFI before the real wake event arrives. */
    exti_interrupt_flag_clear(EXTI_4);
    exti_interrupt_flag_clear(EXTI_8);
    exti_interrupt_flag_clear(EXTI_11);
    exti_interrupt_flag_clear(EXTI_12);
    exti_interrupt_flag_clear(EXTI_3);

    /* PMU_LDO_LOWPOWER stops the APB1 clock in Deep-Sleep, which freezes
     * WWDGT (clocked from PCLK1). Without this, the watchdog keeps counting
     * while the CPU is asleep, hits its 20 ms window, and resets the part —
     * the symptom is a ~25 Hz white LED strobe (boot LED init → sleep → WWDGT
     * reset → repeat). PMU_LDO_NORMAL keeps the APB1 clock running for faster
     * wake-up, but at the cost of that watchdog reset loop. Wake-up latency
     * is a few hundred us longer with LOWPOWER, acceptable for a charger. */
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);

    /* Re-arm PLL and refresh SystemCoreClock so SysTick/USART/I2C keep their
     * pre-sleep frequencies. Without this the wake-side code runs at 8 MHz
     * IRC and every baud/timing calculation is off by 9x. */
    SystemInit();
    SystemCoreClockUpdate();
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
