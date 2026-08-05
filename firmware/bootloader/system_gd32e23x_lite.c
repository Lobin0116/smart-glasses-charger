/* BL minimal SystemInit: runs on the 8 MHz IRC reset default (no PLL).
 * Sets VTOR only. Saves ~1.5 KB vs the full system_gd32e23x.c which pulls
 * the whole RCU clock tree + PLL config that BL doesn't need (FMC and WWDGT
 * run independent of the system clock tree for erase/program timing).
 *
 * BL has plenty of time at 8 MHz: ~3 s for a 27 KB copy, well under any user
 * perceptible delay for a one-shot OTA commit. */

#include "gd32e23x.h"

uint32_t SystemCoreClock = 8000000U;

void SystemInit(void) {
    /* VTOR = flash base; BL vectors at 0x08000000. */
    SCB->VTOR = (uint32_t)0x08000000U;
}
