#include "hal_fwdgt.h"

#include "gd32e23x.h"

/* FWDGT divider steps and their SPL selection values, ascending. The reload
 * counter is 12 bits, so each step doubles the reachable ceiling. */
static const uint8_t fwdgt_presc[7] = {
    FWDGT_PSC_DIV4,  FWDGT_PSC_DIV8,   FWDGT_PSC_DIV16,  FWDGT_PSC_DIV32,
    FWDGT_PSC_DIV64, FWDGT_PSC_DIV128, FWDGT_PSC_DIV256,
};
static const uint32_t fwdgt_div[7] = {4U, 8U, 16U, 32U, 64U, 128U, 256U};

#define FWDGT_IRC_HZ 40000U
#define FWDGT_MAX_RELOAD 0x0FFFU /* 12-bit */

void hal_fwdgt_init(uint32_t timeout_ms) {
    /* timeout_ms = (reload + 1) * div / 40. Solve for the smallest divider that
     * keeps reload within 12 bits for the finest granularity, and round the tick
     * count up so the window is never shorter than requested. */
    if (timeout_ms == 0U) {
        timeout_ms = 1U;
    }

    uint32_t psc_index = 6U; /* fall back to the widest window */
    uint16_t reload = FWDGT_MAX_RELOAD;

    for (uint32_t i = 0U; i < 7U; i++) {
        uint32_t ticks =
            (timeout_ms * FWDGT_IRC_HZ + fwdgt_div[i] * 1000U - 1U) / (fwdgt_div[i] * 1000U);
        if (ticks >= 1U && ticks <= (FWDGT_MAX_RELOAD + 1U)) {
            psc_index = i;
            reload = (uint16_t)(ticks - 1U);
            break;
        }
    }

    /* fwdgt_config programs the divider and reload, then starts the counter. */
    fwdgt_config(reload, fwdgt_presc[psc_index]);
    fwdgt_counter_reload();
}

void hal_fwdgt_feed(void) { fwdgt_counter_reload(); }
