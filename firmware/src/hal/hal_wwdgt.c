#include "hal_wwdgt.h"

#include <stdbool.h>

#include "gd32e23x.h"

static const uint8_t wwdgt_psc_select[4] = {
    WWDGT_CFG_PSC_DIV1, WWDGT_CFG_PSC_DIV2, WWDGT_CFG_PSC_DIV4, WWDGT_CFG_PSC_DIV8
};
static const uint32_t wwdgt_psc_div[4] = {1U, 2U, 4U, 8U};

#define WWDGT_RESET_VALUE 0x40U
#define WWDGT_MAX_COUNTER 0x7FU
#define WWDGT_TICK_DIV    4096U

static uint8_t wwdgt_loaded_counter = WWDGT_MAX_COUNTER;
static bool wwdgt_armed = false;

void hal_wwdgt_init(uint32_t timeout_ms) {
    if (timeout_ms == 0U) {
        timeout_ms = 1U;
    }
    rcu_periph_clock_enable(RCU_WWDGT);

    uint32_t pclk1 = rcu_clock_freq_get(CK_APB1);
    uint32_t best_index = 3U;
    uint32_t best_counter = WWDGT_MAX_COUNTER;

    for (uint32_t i = 0U; i < 4U; i++) {
        uint32_t denom = WWDGT_TICK_DIV * wwdgt_psc_div[i] * 1000U;
        uint32_t ticks = (timeout_ms * pclk1 + denom - 1U) / denom;
        if (ticks >= 1U && ticks <= (WWDGT_MAX_COUNTER - WWDGT_RESET_VALUE)) {
            best_index = i;
            best_counter = WWDGT_RESET_VALUE + ticks;
            break;
        }
    }

    wwdgt_loaded_counter = (uint8_t)best_counter;
    wwdgt_config((uint16_t)best_counter, (uint16_t)best_counter, wwdgt_psc_select[best_index]);
    wwdgt_enable();
    wwdgt_armed = true;
}

void hal_wwdgt_feed(void) {
    if (wwdgt_armed) {
        wwdgt_counter_update(wwdgt_loaded_counter);
    }
}
