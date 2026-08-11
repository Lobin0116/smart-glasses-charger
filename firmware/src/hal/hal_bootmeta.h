#ifndef HAL_BOOTMETA_H
#define HAL_BOOTMETA_H

#include <stdbool.h>
#include <stdint.h>

/* OTA staging boot selector. See docs/OTA_UPGRADE_PLAN.md.
 * Layout (64 KB flash @ 0x08000000, page = 1 KB):
 *   Bootloader: 0x08000000 .. 0x08000FFF  (page 0-3,   4 KB, never erased)
 *   App slot A: 0x08001000 .. 0x08007BFF  (page 4-30, 27 KB, runs here)
 *   Staging B:  0x08007C00 .. 0x0800F7FF  (page 31-61,31 KB, new firmware lands here)
 *   Meta0:      0x0800F800                (page 62)
 *   Meta1:      0x0800FC00                (page 63)
 */

#define BOOT_META_MAGIC   0x4F544131U /* "OTA1" */
#define BOOT_META_ADDR_0  0x0800F800U /* page 62 */
#define BOOT_META_ADDR_1  0x0800FC00U /* page 63 */

#define BOOT_APP_BASE     0x08001000U
#define BOOT_STAGING_BASE 0x08007C00U
#define BOOT_STAGING_SIZE 0x00007C00U /* 31 KB */

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint32_t staged;  /* 0 = no pending fw, 1 = new firmware in Staging B */
    uint32_t fw_size; /* staged firmware size in bytes */
    uint32_t seq;     /* monotonic counter; latest valid meta wins */
    uint32_t crc32;   /* CRC32 over magic..fw_size (first 16 bytes) */
} boot_meta_t;
#pragma pack(pop)

/* Read staged flag and fw_size from the latest valid meta.
 * Returns false if no valid meta (out params zeroed). Safe from SystemInit. */
bool hal_bootmeta_read_staged(bool *staged, uint32_t *fw_size);

/* Mark staged=true with fw_size. Writes the older meta page with seq+1.
 * Called from App ota_run() after writing Staging B. */
bool hal_bootmeta_set_staged(uint32_t fw_size);

/* Mark staged=false (App slot now has the new firmware). Writes the older meta
 * page with seq+1. Called from Bootloader after copying Staging → App. */
bool hal_bootmeta_clear_staged(void);

#endif /* HAL_BOOTMETA_H */
