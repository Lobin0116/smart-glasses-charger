#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_FLASH_PAGE_SIZE 0x400U

void hal_flash_unlock(void);

void hal_flash_lock(void);

bool hal_flash_page_erase(uint32_t page_address);

bool hal_flash_write(uint32_t address, const uint8_t *data, uint32_t len);

bool hal_flash_read(uint32_t address, uint8_t *buf, uint32_t len);

#endif
