#ifndef HAL_BOOTMETA_H
#define HAL_BOOTMETA_H

#include <stdbool.h>
#include <stdint.h>
#define BOOT_META_MAGIC 0x4F544131U
#define BOOT_META_ADDR_0 0x0800F800U
#define BOOT_META_ADDR_1 0x0800FC00U

#define BOOT_APP_BASE 0x08001000U
#define BOOT_STAGING_BASE 0x08007C00U
#define BOOT_STAGING_SIZE 0x00007C00U

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint32_t staged;
    uint32_t fw_size;
    uint32_t seq;
    uint32_t crc32;
} boot_meta_t;
#pragma pack(pop)

bool hal_bootmeta_read_staged(bool *staged, uint32_t *fw_size);

bool hal_bootmeta_set_staged(uint32_t fw_size);

bool hal_bootmeta_clear_staged(void);

#endif
