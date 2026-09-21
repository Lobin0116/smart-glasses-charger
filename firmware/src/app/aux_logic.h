#ifndef AUX_LOGIC_H
#define AUX_LOGIC_H

#include "state_machine.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    NTC_NORMAL,
    NTC_COLD,
    NTC_MILD,
    NTC_WARM,
    NTC_CRITICAL
} ntc_zone_t;

ntc_zone_t ntc_get_zone(int8_t temp_c);
bool ntc_should_reduce_charge(ntc_zone_t zone);
bool ntc_should_stop_charge(ntc_zone_t zone);

bool recharge_check(uint8_t glass_soc, bool glass_full);

bool lid_check_glass_present(sm_ctx_t *ctx);
void lid_no_glass_display(uint8_t case_soc);

#endif
