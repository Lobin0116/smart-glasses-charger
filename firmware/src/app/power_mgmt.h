#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdbool.h>

/* Low-power management for the GD32E230. The case spends most of its life
 * asleep: Deep-Sleep between activity, woken by any EXTI edge, and Standby when
 * shut down or shipped. Every entry blocks until the device wakes; Standby in
 * fact never returns, because the wake restarts execution from the reset
 * vector. */

/* Enter Deep-Sleep until an EXTI edge (hall, key, BAT_INT, CHARGER_INT or
 * COIL_INT) wakes the core. SRAM and peripheral state are retained on the
 * GD32E230, so on wake execution resumes at the caller without re-running
 * SystemInit. The wake lines are the EXTI sources armed by hal_exti_init(). */
void pm_enter_deep_sleep(void);

/* Cut the external load (SHIP_CTR high) and enter Standby. Only NRST or the
 * WKUP pin can bring the device back, and a Standby wake behaves as a reset, so
 * this function does not return. */
void pm_enter_standby(void);

/* Shipping/storage mode: cut external power and enter Standby for the lowest
 * drain until the unit is first powered on. Equivalent to Standby but used to
 * signal the long-term storage intent. */
void pm_enter_ship_mode(void);

/* Boot-time check of the PMU flags. Returns true when this image was woken from
 * Deep-Sleep by an external interrupt (normal operation), false when it is a
 * fresh reset (power-on, NRST, or Standby wake). The flags are cleared once
 * read so the result reflects only the most recent wake. */
bool pm_check_wakeup_reason(void);

#endif /* POWER_MGMT_H */
