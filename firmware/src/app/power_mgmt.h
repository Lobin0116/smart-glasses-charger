#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdbool.h>
void pm_enter_deep_sleep(void);

void pm_enter_standby(void);

void pm_enter_ship_mode(void);

bool pm_check_wakeup_reason(void);

#endif
