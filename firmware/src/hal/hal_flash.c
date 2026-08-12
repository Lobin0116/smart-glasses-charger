#include "hal_flash.h"

#include <string.h>

#include "gd32e23x.h"
#include "gd32e23x_fmc.h"
#ifndef BL_NO_WWDGT
    #include "hal_wwdgt.h"
#endif

#define FMC_FLAG_ALL (FMC_FLAG_END | FMC_FLAG_PGERR | FMC_FLAG_PGAERR | FMC_FLAG_WPERR)

void hal_flash_unlock(void) { fmc_unlock(); }

void hal_flash_lock(void) { fmc_lock(); }

bool hal_flash_page_erase(uint32_t page_address)
{
    fmc_state_enum state;
    __disable_irq();
    state = fmc_page_erase(page_address);
    fmc_flag_clear(FMC_FLAG_ALL);
    __enable_irq();
#ifndef BL_NO_WWDGT
    hal_wwdgt_feed();
#endif
    return state == FMC_READY;
}

bool hal_flash_write(uint32_t address, const uint8_t *data, uint32_t len)
{
    if ((address & 0x3U) != 0U || (len & 0x3U) != 0U || len == 0U) {
        return false;
    }

    __disable_irq();
    for (uint32_t i = 0U; i < len; i += 4U) {
        uint32_t word;
        memcpy(&word, data + i, sizeof(word));
        fmc_state_enum state = fmc_word_program(address + i, word);
        fmc_flag_clear(FMC_FLAG_ALL);
        if (state != FMC_READY) {
            __enable_irq();
            return false;
        }
        /* Feed WWDGT roughly every 1 KB (256 word programs ~ 11 ms). */
        if (((i + 4U) & 0x3FFU) == 0U) {
            __enable_irq();
#ifndef BL_NO_WWDGT
            hal_wwdgt_feed();
#endif
            __disable_irq();
        }
    }
    __enable_irq();
#ifndef BL_NO_WWDGT
    hal_wwdgt_feed();
#endif
    return true;
}

bool hal_flash_read(uint32_t address, uint8_t *buf, uint32_t len)
{
    if (buf == NULL) {
        return false;
    }
    /* Main flash is memory-mapped on the GD32; a plain byte copy is enough.
     * Caller guarantees address..address+len is within flash. */
    memcpy(buf, (const void *)address, len);
    return true;
}
