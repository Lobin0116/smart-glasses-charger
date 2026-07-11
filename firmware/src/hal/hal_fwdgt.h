#ifndef HAL_FWDGT_H
#define HAL_FWDGT_H

#include <stdint.h>

/* Independent watchdog (GD32 FWDGT). It clocks from the on-chip 40 kHz IRC and
 * keeps counting in Deep-Sleep, so feeding it from the main loop guards against
 * a stalled super-loop regardless of the low-power mode. Once enabled it runs
 * until the next reset. */

/* Arm the watchdog with an approximate timeout_ms window. The value is rounded
 * up to the nearest count the 40 kHz / prescaler / 12-bit reload combo can
 * express and clamped to the ~26 s maximum. Call once at startup. */
void hal_fwdgt_init(uint32_t timeout_ms);

/* Restart the countdown. Call from the main loop faster than the configured
 * timeout or the MCU resets. */
void hal_fwdgt_feed(void);

#endif /* HAL_FWDGT_H */
