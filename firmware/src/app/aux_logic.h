#ifndef AUX_LOGIC_H
#define AUX_LOGIC_H

#include "state_machine.h"
#include <stdbool.h>
#include <stdint.h>

/* Task 25: NTC temperature protection */
typedef enum
{
    NTC_NORMAL,
    NTC_COLD,    /* 0 ~ <15C: reduced charge */
    NTC_MILD,    /* 15 ~ <45C: full charge */
    NTC_WARM,    /* 45 ~ 60C: reduced charge */
    NTC_CRITICAL /* >60C: stop charge */
} ntc_zone_t;

ntc_zone_t ntc_get_zone(int8_t temp_c);
bool ntc_should_reduce_charge(ntc_zone_t zone);
bool ntc_should_stop_charge(ntc_zone_t zone);

/* Wired/wireless arbitration (old Task 26) was deleted with the V1 board:
 * on V2 the USB 5V and the NU1671 RX output are OR'd in hardware onto
 * VBUS_IN, and the NU1671 is self-powered from the coil — there is nothing
 * for software to arbitrate or enable. */

/* Task 27: Recharge logic */
bool recharge_check(uint8_t glass_soc, bool glass_full);

/* Task 28+30: Lid event helpers */
bool lid_check_glass_present(sm_ctx_t *ctx);
void lid_no_glass_display(uint8_t case_soc);

#endif
