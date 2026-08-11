#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdbool.h>
#include <stdint.h>

/* GD32E230xx FMC page size = 1 KB. See docs/OTA_UPGRADE_PLAN.md §3. */
#define HAL_FLASH_PAGE_SIZE 0x400U

/* Unlock main flash for erase/program. Pair with hal_flash_lock().
 * Wraps SPL fmc_unlock(). */
void hal_flash_unlock(void);

/* Re-lock main flash. */
void hal_flash_lock(void);

/* Erase one 1 KB page. page_address must be page-aligned.
 * Returns true on FMC_READY, false on FMC error.
 * Disables IRQ around the FMC operation (flash bus stalls during erase). */
bool hal_flash_page_erase(uint32_t page_address);

/* Program len bytes to address from data. Both must be 4-byte aligned
 * (FMC word program constraint). Returns true on full success.
 * Feeds WWDGT every 1 KB to avoid watchdog reset during long writes. */
bool hal_flash_write(uint32_t address, const uint8_t *data, uint32_t len);

#endif /* HAL_FLASH_H */
