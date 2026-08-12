/* Unit tests for hal_bootmeta — OTA boot selector.
 *
 * hal_bootmeta writes a small descriptor (magic + staged + fw_size + seq + CRC32)
 * to one of two flash pages and picks the latest valid one on boot. The dual-page
 * rotation survives power loss mid-write (one page may corrupt, the other still
 * holds the previous valid record). HIL can't reproduce mid-write power cuts, so
 * this suite covers the logic by injecting every corruption / sequencing case
 * into a RAM-backed mock of pages 62-63.
 *
 * Coverage map:
 *   pick_latest  : empty, m0-only, m1-only, both s0>=s1, both s0<s1
 *   meta_validate: bad magic, bad CRC
 *   set_staged   : first write lands at m0 seq=0, then alternates to m1, m0, ...
 *   clear_staged : preserves fw_size, still rotates pages
 *   OTA recovery : set=true reads back true (BL must copy); cleared reads false
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal_bootmeta.h"
#include "hal_flash.h"
#include "test_assert.h"

/* RAM-backed mock of meta pages 62 + 63. Real addresses BOOT_META_ADDR_0/1 are
 * mapped into a 2 KB buffer. Erased state = 0xFF, flash write ANDs bits (1→0
 * only), erase resets a 1 KB page to 0xFF — mirrors NOR flash semantics closely
 * enough that the erase-then-write cycle behaves identically.
 *
 * hal_bootmeta now reads via hal_flash_read (instead of dereferencing the
 * memory-mapped address), so this mock is sufficient — no address overrides. */
#define MOCK_SIZE 0x800U /* both 1 KB pages */
static uint8_t mock_flash[MOCK_SIZE];

void hal_flash_unlock(void) {}
void hal_flash_lock(void) {}

bool hal_flash_page_erase(uint32_t page_address)
{
    if (page_address < BOOT_META_ADDR_0 || page_address >= BOOT_META_ADDR_0 + MOCK_SIZE) {
        return false;
    }
    uint32_t off = page_address - BOOT_META_ADDR_0;
    uint32_t page_start = off & ~((uint32_t)HAL_FLASH_PAGE_SIZE - 1U);
    memset(&mock_flash[page_start], 0xFF, HAL_FLASH_PAGE_SIZE);
    return true;
}

bool hal_flash_write(uint32_t address, const uint8_t *data, uint32_t len)
{
    if (address < BOOT_META_ADDR_0 || address + len > BOOT_META_ADDR_0 + MOCK_SIZE) {
        return false;
    }
    uint32_t off = address - BOOT_META_ADDR_0;
    for (uint32_t i = 0U; i < len; i++) {
        mock_flash[off + i] &= data[i]; /* NOR: bits only clear */
    }
    return true;
}

bool hal_flash_read(uint32_t address, uint8_t *buf, uint32_t len)
{
    if (address < BOOT_META_ADDR_0 || address + len > BOOT_META_ADDR_0 + MOCK_SIZE) {
        return false;
    }
    uint32_t off = address - BOOT_META_ADDR_0;
    memcpy(buf, &mock_flash[off], len);
    return true;
}

/* ----- helpers ----- */

static void flash_reset(void)
{
    memset(mock_flash, 0xFF, MOCK_SIZE);
}

/* Replicates bootmeta_crc32 (table-less IEEE 802.3) so tests can construct
 * records that meta_validate will accept, and intentionally corrupt the CRC
 * field to inject faults. */
static uint32_t meta_crc32(const uint8_t *data, uint32_t len)
{
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

static boot_meta_t make_valid_meta(uint32_t staged, uint32_t fw_size, uint32_t seq)
{
    boot_meta_t m;
    m.magic = BOOT_META_MAGIC;
    m.staged = staged;
    m.fw_size = fw_size;
    m.seq = seq;
    m.crc32 = meta_crc32((const uint8_t *)&m, (uint32_t)offsetof(boot_meta_t, crc32));
    return m;
}

/* Write a complete meta record at the given page (erase-then-program, matching
 * what hal_bootmeta_set_staged does internally). */
static void write_meta_at(uint32_t addr, const boot_meta_t *m)
{
    (void)hal_flash_page_erase(addr);
    (void)hal_flash_write(addr, (const uint8_t *)m, (uint32_t)sizeof(*m));
}

/* Read back the meta record at addr from the mock buffer. */
static boot_meta_t meta_at(uint32_t addr)
{
    boot_meta_t m;
    (void)hal_flash_read(addr, (uint8_t *)&m, sizeof(m));
    return m;
}

/* ----- tests ----- */

static void test_empty_returns_false(void)
{
    flash_reset();
    bool staged = true;
    uint32_t fw_size = 12345U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(!ok, "empty flash: read_staged should return false");
    TEST_ASSERT(!staged, "empty flash: staged should be false");
    TEST_ASSERT_EQ(fw_size, 0U, "empty flash: fw_size should be 0");
}

static void test_meta0_only_reads_staged(void)
{
    flash_reset();
    boot_meta_t m = make_valid_meta(1U, 4096U, 3U);
    write_meta_at(BOOT_META_ADDR_0, &m);

    bool staged = false;
    uint32_t fw_size = 0U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(ok, "m0 valid: read should succeed");
    TEST_ASSERT(staged, "m0 valid: staged should be true");
    TEST_ASSERT_EQ(fw_size, 4096U, "m0 valid: fw_size mismatch");
}

static void test_meta1_only_reads_staged(void)
{
    flash_reset();
    boot_meta_t m = make_valid_meta(1U, 8192U, 7U);
    write_meta_at(BOOT_META_ADDR_1, &m);

    bool staged = false;
    uint32_t fw_size = 0U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(ok, "m1 valid: read should succeed");
    TEST_ASSERT(staged, "m1 valid: staged should be true");
    TEST_ASSERT_EQ(fw_size, 8192U, "m1 valid: fw_size mismatch");
}

static void test_both_valid_picks_higher_seq_m0(void)
{
    flash_reset();
    boot_meta_t m0 = make_valid_meta(0U, 1000U, 10U);
    boot_meta_t m1 = make_valid_meta(1U, 2000U, 5U); /* lower seq, but staged=1 trap */
    write_meta_at(BOOT_META_ADDR_0, &m0);
    write_meta_at(BOOT_META_ADDR_1, &m1);

    bool staged = true;
    uint32_t fw_size = 0U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(ok, "both valid: read should succeed");
    TEST_ASSERT_EQ(fw_size, 1000U, "s0>s1: should pick m0's fw_size");
    TEST_ASSERT(!staged, "s0>s1: should pick m0's staged=false");
}

static void test_both_valid_picks_higher_seq_m1(void)
{
    flash_reset();
    boot_meta_t m0 = make_valid_meta(1U, 1000U, 5U);
    boot_meta_t m1 = make_valid_meta(1U, 2000U, 10U); /* higher seq */
    write_meta_at(BOOT_META_ADDR_0, &m0);
    write_meta_at(BOOT_META_ADDR_1, &m1);

    bool staged = false;
    uint32_t fw_size = 0U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(ok, "both valid: read should succeed");
    TEST_ASSERT_EQ(fw_size, 2000U, "s0<s1: should pick m1's fw_size");
    TEST_ASSERT(staged, "s0<s1: should pick m1's staged=true");
}

static void test_corrupt_crc_skipped(void)
{
    flash_reset();
    boot_meta_t m0 = make_valid_meta(1U, 4096U, 3U);
    m0.crc32 ^= 0xDEADBEEFU; /* corrupt CRC */
    write_meta_at(BOOT_META_ADDR_0, &m0);

    bool staged = true;
    uint32_t fw_size = 99U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(!ok, "bad CRC: read should return false (no valid meta)");
    TEST_ASSERT(!staged, "bad CRC: staged should be false");
    TEST_ASSERT_EQ(fw_size, 0U, "bad CRC: fw_size should be 0");
}

static void test_corrupt_magic_skipped(void)
{
    flash_reset();
    boot_meta_t m0 = make_valid_meta(1U, 4096U, 3U);
    m0.magic = 0x12345678U; /* bogus magic */
    m0.crc32 = meta_crc32((const uint8_t *)&m0, (uint32_t)offsetof(boot_meta_t, crc32));
    write_meta_at(BOOT_META_ADDR_0, &m0);

    bool staged = true;
    uint32_t fw_size = 99U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(!ok, "bad magic: read should return false");
}

static void test_corrupt_one_page_falls_back_to_other(void)
{
    /* Power-loss scenario: m0 was the previous good record, write to m1 got
     * interrupted mid-program leaving m1 with valid magic but garbage CRC.
     * Boot must still find m0 and use it. */
    flash_reset();
    boot_meta_t m0 = make_valid_meta(0U, 5000U, 4U);
    write_meta_at(BOOT_META_ADDR_0, &m0);

    /* Partially-write m1: erase page then write only the magic word + part of
     * the payload, simulating a torn write that leaves CRC mismatched. */
    boot_meta_t m1_attempt = make_valid_meta(1U, 6000U, 5U);
    m1_attempt.crc32 = 0xFFFFFFFFU; /* deliberately wrong (would-be in-progress) */
    write_meta_at(BOOT_META_ADDR_1, &m1_attempt);

    bool staged = true;
    uint32_t fw_size = 0U;
    bool ok = hal_bootmeta_read_staged(&staged, &fw_size);
    TEST_ASSERT(ok, "torn-write: read should still succeed via m0");
    TEST_ASSERT_EQ(fw_size, 5000U, "torn-write: should fall back to m0's fw_size");
    TEST_ASSERT(!staged, "torn-write: should report m0's staged=false");
}

static void test_set_staged_first_write_lands_at_m0_seq0(void)
{
    flash_reset();
    bool ok = hal_bootmeta_set_staged(2048U);
    TEST_ASSERT(ok, "set_staged: should succeed");

    /* m0 must hold the new record; m1 stays erased. */
    boot_meta_t m0 = meta_at(BOOT_META_ADDR_0);
    boot_meta_t m1 = meta_at(BOOT_META_ADDR_1);
    TEST_ASSERT_EQ(m0.magic, BOOT_META_MAGIC, "set_staged: m0 magic");
    TEST_ASSERT_EQ(m0.staged, 1U, "set_staged: m0 staged");
    TEST_ASSERT_EQ(m0.fw_size, 2048U, "set_staged: m0 fw_size");
    TEST_ASSERT_EQ(m0.seq, 0U, "set_staged: first write seq=0");
    TEST_ASSERT_EQ(m1.magic, 0xFFFFFFFFU, "set_staged: m1 untouched (erased)");
}

static void test_set_clear_alternates_pages(void)
{
    /* Sequence: set → clear → set → clear. Each call must rotate to the other
     * page (older one overwritten) and bump seq, so neither page accumulates
     * stale data and pick_latest always picks the newest. */
    flash_reset();

    /* 1) set → m0 seq=0 staged=1 fw=1000 */
    TEST_ASSERT(hal_bootmeta_set_staged(1000U), "set #1");
    boot_meta_t m0 = meta_at(BOOT_META_ADDR_0);
    boot_meta_t m1 = meta_at(BOOT_META_ADDR_1);
    TEST_ASSERT_EQ(m0.staged, 1U, "after set #1: m0 staged");
    TEST_ASSERT_EQ(m0.seq, 0U, "after set #1: m0 seq");
    TEST_ASSERT_EQ(m1.magic, 0xFFFFFFFFU, "after set #1: m1 erased");

    /* 2) clear → m1 seq=1 staged=0 fw=1000 (preserved) */
    TEST_ASSERT(hal_bootmeta_clear_staged(), "clear #1");
    m0 = meta_at(BOOT_META_ADDR_0);
    m1 = meta_at(BOOT_META_ADDR_1);
    TEST_ASSERT_EQ(m1.staged, 0U, "after clear #1: m1 staged=false");
    TEST_ASSERT_EQ(m1.fw_size, 1000U, "after clear #1: m1 fw_size preserved");
    TEST_ASSERT_EQ(m1.seq, 1U, "after clear #1: m1 seq=1");

    /* 3) set → m0 seq=2 staged=1 fw=2000 */
    TEST_ASSERT(hal_bootmeta_set_staged(2000U), "set #2");
    m0 = meta_at(BOOT_META_ADDR_0);
    TEST_ASSERT_EQ(m0.staged, 1U, "after set #2: m0 staged");
    TEST_ASSERT_EQ(m0.fw_size, 2000U, "after set #2: m0 fw_size");
    TEST_ASSERT_EQ(m0.seq, 2U, "after set #2: m0 seq=2");

    /* 4) clear → m1 seq=3 staged=0 fw=2000 */
    TEST_ASSERT(hal_bootmeta_clear_staged(), "clear #2");
    m1 = meta_at(BOOT_META_ADDR_1);
    TEST_ASSERT_EQ(m1.staged, 0U, "after clear #2: m1 staged=false");
    TEST_ASSERT_EQ(m1.fw_size, 2000U, "after clear #2: m1 fw_size preserved");
    TEST_ASSERT_EQ(m1.seq, 3U, "after clear #2: m1 seq=3");

    /* Final state: pick_latest must return m1 (seq=3, staged=false). */
    bool staged = true;
    uint32_t fw_size = 0U;
    TEST_ASSERT(hal_bootmeta_read_staged(&staged, &fw_size), "final read");
    TEST_ASSERT(!staged, "final: staged=false");
    TEST_ASSERT_EQ(fw_size, 2000U, "final: fw_size from latest");
}

static void test_clear_on_empty_preserves_zero_fw_size(void)
{
    /* Edge: clearing when nothing was ever staged. pick_latest=NULL so fw_size
     * defaults to 0; write_meta_at writes staged=0 fw_size=0 seq=0. */
    flash_reset();
    bool ok = hal_bootmeta_clear_staged();
    TEST_ASSERT(ok, "clear on empty: should still succeed");

    bool staged = true;
    uint32_t fw_size = 99U;
    TEST_ASSERT(hal_bootmeta_read_staged(&staged, &fw_size), "after clear: read");
    TEST_ASSERT(!staged, "after clear: staged=false");
    TEST_ASSERT_EQ(fw_size, 0U, "after clear: fw_size=0");
}

static void test_set_then_read_round_trip(void)
{
    /* End-to-end OTA commit simulation: write Staging → set_staged → reboot →
     * BL reads staged=true → (BL copies Staging → App) → clear_staged →
     * subsequent boots read staged=false. */
    flash_reset();

    TEST_ASSERT(hal_bootmeta_set_staged(0x5000U), "OTA commit: set_staged");

    bool staged = false;
    uint32_t fw_size = 0U;
    TEST_ASSERT(hal_bootmeta_read_staged(&staged, &fw_size), "post-commit read");
    TEST_ASSERT(staged, "post-commit: staged must be true (BL should copy)");
    TEST_ASSERT_EQ(fw_size, 0x5000U, "post-commit: fw_size");

    TEST_ASSERT(hal_bootmeta_clear_staged(), "BL clear_staged");

    staged = true;
    TEST_ASSERT(hal_bootmeta_read_staged(&staged, &fw_size), "post-clear read");
    TEST_ASSERT(!staged, "post-clear: staged must be false (no more copies)");
}

int main(void)
{
    TEST_RUN(test_empty_returns_false);
    TEST_RUN(test_meta0_only_reads_staged);
    TEST_RUN(test_meta1_only_reads_staged);
    TEST_RUN(test_both_valid_picks_higher_seq_m0);
    TEST_RUN(test_both_valid_picks_higher_seq_m1);
    TEST_RUN(test_corrupt_crc_skipped);
    TEST_RUN(test_corrupt_magic_skipped);
    TEST_RUN(test_corrupt_one_page_falls_back_to_other);
    TEST_RUN(test_set_staged_first_write_lands_at_m0_seq0);
    TEST_RUN(test_set_clear_alternates_pages);
    TEST_RUN(test_clear_on_empty_preserves_zero_fw_size);
    TEST_RUN(test_set_then_read_round_trip);
    TEST_SUMMARY();
}
