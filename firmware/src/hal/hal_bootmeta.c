#include "hal_bootmeta.h"

#include <stddef.h>

#include "hal_flash.h"

/* Software CRC32 (IEEE 802.3, poly 0xEDB88320, init 0xFFFFFFFF, final XOR).
 * Table-less to stay callable from Bootloader main (no .data dependencies). */
static uint32_t bootmeta_crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t b = 0U; b < 8U; b++) {
            uint32_t mask = (crc & 1U) ? 0xEDB88320U : 0U;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

static bool meta_validate(const boot_meta_t *m, uint32_t *seq_out) {
    if (m->magic != BOOT_META_MAGIC) {
        return false;
    }
    uint32_t expected = bootmeta_crc32((const uint8_t *)m, (uint32_t)offsetof(boot_meta_t, crc32));
    if (m->crc32 != expected) {
        return false;
    }
    if (seq_out != NULL) {
        *seq_out = m->seq;
    }
    return true;
}

static const boot_meta_t *pick_latest(uint32_t *seq_out) {
    const boot_meta_t *m0 = (const boot_meta_t *)BOOT_META_ADDR_0;
    const boot_meta_t *m1 = (const boot_meta_t *)BOOT_META_ADDR_1;
    uint32_t s0 = 0U, s1 = 0U;
    bool v0 = meta_validate(m0, &s0);
    bool v1 = meta_validate(m1, &s1);
    if (!v0 && !v1) {
        return NULL;
    }
    if (!v0) {
        if (seq_out != NULL) {
            *seq_out = s1;
        }
        return m1;
    }
    if (!v1) {
        if (seq_out != NULL) {
            *seq_out = s0;
        }
        return m0;
    }
    if (s0 >= s1) {
        if (seq_out != NULL) {
            *seq_out = s0;
        }
        return m0;
    }
    if (seq_out != NULL) {
        *seq_out = s1;
    }
    return m1;
}

bool hal_bootmeta_read_staged(bool *staged, uint32_t *fw_size) {
    const boot_meta_t *m = pick_latest(NULL);
    if (m == NULL) {
        if (staged != NULL) {
            *staged = false;
        }
        if (fw_size != NULL) {
            *fw_size = 0U;
        }
        return false;
    }
    if (staged != NULL) {
        *staged = (m->staged != 0U);
    }
    if (fw_size != NULL) {
        *fw_size = m->fw_size;
    }
    return true;
}

static bool write_meta_at(uint32_t target_addr, uint32_t staged, uint32_t fw_size, uint32_t seq) {
    boot_meta_t nm;
    nm.magic = BOOT_META_MAGIC;
    nm.staged = staged;
    nm.fw_size = fw_size;
    nm.seq = seq;
    nm.crc32 = bootmeta_crc32((const uint8_t *)&nm, (uint32_t)offsetof(boot_meta_t, crc32));

    hal_flash_unlock();
    bool ok = hal_flash_page_erase(target_addr);
    if (ok) {
        ok = hal_flash_write(target_addr, (const uint8_t *)&nm, (uint32_t)sizeof(nm));
    }
    hal_flash_lock();
    return ok;
}

/* Pick the older meta slot to overwrite (lower seq, or the invalid one).
 * Returns target address and the next seq to write. */
static uint32_t next_meta_target(uint32_t *new_seq_out) {
    const boot_meta_t *m0 = (const boot_meta_t *)BOOT_META_ADDR_0;
    const boot_meta_t *m1 = (const boot_meta_t *)BOOT_META_ADDR_1;
    uint32_t s0 = 0U, s1 = 0U;
    bool v0 = meta_validate(m0, &s0);
    bool v1 = meta_validate(m1, &s1);
    uint32_t new_seq;
    uint32_t target_addr;
    if (!v0 && !v1) {
        new_seq = 0U;
        target_addr = BOOT_META_ADDR_0;
    } else if (!v0) {
        new_seq = s1 + 1U;
        target_addr = BOOT_META_ADDR_0;
    } else if (!v1) {
        new_seq = s0 + 1U;
        target_addr = BOOT_META_ADDR_1;
    } else if (s0 <= s1) {
        new_seq = s1 + 1U;
        target_addr = BOOT_META_ADDR_0;
    } else {
        new_seq = s0 + 1U;
        target_addr = BOOT_META_ADDR_1;
    }
    if (new_seq_out != NULL) {
        *new_seq_out = new_seq;
    }
    return target_addr;
}

bool hal_bootmeta_set_staged(uint32_t fw_size) {
    uint32_t new_seq = 0U;
    uint32_t target = next_meta_target(&new_seq);
    return write_meta_at(target, 1U, fw_size, new_seq);
}

bool hal_bootmeta_clear_staged(void) {
    uint32_t fw_size = 0U;
    const boot_meta_t *latest = pick_latest(NULL);
    if (latest != NULL) {
        fw_size = latest->fw_size;
    }
    uint32_t new_seq = 0U;
    uint32_t target = next_meta_target(&new_seq);
    return write_meta_at(target, 0U, fw_size, new_seq);
}
