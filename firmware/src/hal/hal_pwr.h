#ifndef HAL_PWR_H
#define HAL_PWR_H

#include <stdint.h>
void hal_pwr_enter_charge(void);

void hal_pwr_enter_comm(void);

void hal_pwr_pulse_charge(uint32_t ms);

void hal_pwr_discharge(uint32_t ms);

void hal_pwr_idle(void);

#endif
