#ifndef HAL_PWR_H
#define HAL_PWR_H

#include <stdint.h>

/* ET3328 analog-switch driver for the POGO link. The switch routes the POGO pin
 * between the 5V charge rail and the 1.8V UART transceiver: PB13 (IN) selects the
 * path, PB15 (RPD) bleeds the bus, and PB10 (1V8EN) gates the 1.8V LDO. Pin setup
 * is owned by hal_gpio; this layer only sequences those three controls. */

/* Route the POGO pin to the 5V charge rail: IN=L, RPD=L, 1V8EN=L. The boost that
 * actually sources the 5V is owned by the IP5353 driver. */
void hal_pwr_enter_charge(void);

/* Switch the POGO pin from charge to UART: discharge the bus for the timing
 * spec's 100ms floor, enable the 1.8V LDO, then steer the switch to the UART
 * path. UART traffic is valid once this returns. */
void hal_pwr_enter_comm(void);

/* Briefly present 5V on the POGO pin for ms milliseconds (the handshake wake
 * pulse), then restore the previous mode. */
void hal_pwr_pulse_charge(uint32_t ms);

/* Hold RPD high for ms milliseconds to bleed the POGO bus, then release it. The
 * current mode is unchanged. */
void hal_pwr_discharge(uint32_t ms);

/* Drop every control low: the switch sits in the charge position but neither the
 * 1.8V LDO nor the 5V boost is energised. */
void hal_pwr_idle(void);

#endif /* HAL_PWR_H */
