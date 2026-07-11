/*
 * Unit tests for state machine transitions and charge_flow helpers.
 *
 * The state machine is mostly timing-driven and calls hardware functions,
 * so we focus on:
 * 1. The byte-building helpers (pure logic, fully testable)
 * 2. State transition logic with mocked timing
 * 3. aux_logic decision functions
 */

#include "test_assert.h"

/* We need to mock hal_timer to control time progression */
#include "mock_spl.h"

/* --- charge_flow byte building (pure logic) --- */

#include "charge_flow.h"

static void test_build_case_soc_charging(void) {
    uint8_t b = sm_build_case_soc_byte(50, true);
    TEST_ASSERT(b & 0x80, "Charging bit should be set");
    TEST_ASSERT_EQ((b & 0x7F), 50, "SOC should be 50");
}

static void test_build_case_soc_not_charging(void) {
    uint8_t b = sm_build_case_soc_byte(30, false);
    TEST_ASSERT(!(b & 0x80), "Charging bit should be clear");
    TEST_ASSERT_EQ((b & 0x7F), 30, "SOC should be 30");
}

static void test_build_case_soc_max(void) {
    uint8_t b = sm_build_case_soc_byte(100, true);
    TEST_ASSERT(b & 0x80, "Charging bit should be set");
    TEST_ASSERT_EQ((b & 0x7F), 100, "SOC should be 100");
}

static void test_build_case_soc_zero(void) {
    uint8_t b = sm_build_case_soc_byte(0, false);
    TEST_ASSERT_EQ(b, 0, "Zero SOC not charging should be 0x00");
}

static void test_build_case_sta_lid_open(void) {
    uint8_t b = sm_build_case_sta_byte(true, false);
    TEST_ASSERT(b & 0x01, "Lid open bit should be set");
    TEST_ASSERT(!(b & 0x80), "OTA bit should be clear");
}

static void test_build_case_sta_lid_closed(void) {
    uint8_t b = sm_build_case_sta_byte(false, false);
    TEST_ASSERT_EQ(b, 0, "Closed lid, no OTA should be 0x00");
}

static void test_build_case_sta_ota(void) {
    uint8_t b = sm_build_case_sta_byte(false, true);
    TEST_ASSERT(b & 0x80, "OTA bit should be set");
    TEST_ASSERT(!(b & 0x01), "Lid bit should be clear");
}

static void test_build_case_sta_all(void) {
    uint8_t b = sm_build_case_sta_byte(true, true);
    TEST_ASSERT(b & 0x01, "Lid open should be set");
    TEST_ASSERT(b & 0x80, "OTA should be set");
}

/* --- aux_logic decision functions (pure logic) --- */

#include "aux_logic.h"

static void test_ntc_normal(void) {
    TEST_ASSERT_EQ(ntc_get_zone(25), NTC_MILD, "25C should be mild");
    TEST_ASSERT_EQ(ntc_get_zone(20), NTC_MILD, "20C should be mild");
    TEST_ASSERT_EQ(ntc_get_zone(44), NTC_MILD, "44C should be mild");
}

static void test_ntc_cold(void) {
    TEST_ASSERT_EQ(ntc_get_zone(0), NTC_COLD, "0C should be cold");
    TEST_ASSERT_EQ(ntc_get_zone(14), NTC_COLD, "14C should be cold");
    TEST_ASSERT_EQ(ntc_get_zone(-10), NTC_COLD, "-10C should be cold");
}

static void test_ntc_warm(void) {
    TEST_ASSERT_EQ(ntc_get_zone(45), NTC_WARM, "45C should be warm");
    TEST_ASSERT_EQ(ntc_get_zone(60), NTC_WARM, "60C should be warm");
}

static void test_ntc_critical(void) {
    TEST_ASSERT_EQ(ntc_get_zone(61), NTC_CRITICAL, "61C should be critical");
    TEST_ASSERT_EQ(ntc_get_zone(80), NTC_CRITICAL, "80C should be critical");
}

static void test_ntc_reduce(void) {
    TEST_ASSERT(ntc_should_reduce_charge(NTC_COLD), "Cold should reduce");
    TEST_ASSERT(ntc_should_reduce_charge(NTC_WARM), "Warm should reduce");
    TEST_ASSERT(!ntc_should_reduce_charge(NTC_MILD), "Mild should not reduce");
    TEST_ASSERT(!ntc_should_reduce_charge(NTC_CRITICAL), "Critical not reduce, it stops");
}

static void test_ntc_stop(void) {
    TEST_ASSERT(ntc_should_stop_charge(NTC_CRITICAL), "Critical should stop");
    TEST_ASSERT(!ntc_should_stop_charge(NTC_NORMAL), "Normal should not stop");
    TEST_ASSERT(!ntc_should_stop_charge(NTC_WARM), "Warm should not stop");
}

static void test_recharge_needed(void) {
    TEST_ASSERT(recharge_check(95, false), "95% not full should recharge");
    TEST_ASSERT(recharge_check(50, false), "50% should recharge");
}

static void test_recharge_not_needed(void) {
    TEST_ASSERT(!recharge_check(99, false), "99% should not recharge (< 98 threshold)");
    TEST_ASSERT(!recharge_check(50, true), "Full should not recharge");
    TEST_ASSERT(!recharge_check(0, true), "Full flag overrides SOC");
}

static void test_charge_arbitrate(void) {
    TEST_ASSERT_EQ(charge_arbitrate(true, false), CHARGE_SRC_USB, "USB only");
    TEST_ASSERT_EQ(charge_arbitrate(true, true), CHARGE_SRC_USB, "USB priority over wireless");
    TEST_ASSERT_EQ(charge_arbitrate(false, true), CHARGE_SRC_WIRELESS, "Wireless only");
    TEST_ASSERT_EQ(charge_arbitrate(false, false), CHARGE_SRC_NONE, "No source");
}

int main(void) {
    printf("=== Charge Flow + Aux Logic Unit Tests ===\n\n");

    TEST_RUN(test_build_case_soc_charging);
    TEST_RUN(test_build_case_soc_not_charging);
    TEST_RUN(test_build_case_soc_max);
    TEST_RUN(test_build_case_soc_zero);
    TEST_RUN(test_build_case_sta_lid_open);
    TEST_RUN(test_build_case_sta_lid_closed);
    TEST_RUN(test_build_case_sta_ota);
    TEST_RUN(test_build_case_sta_all);

    TEST_RUN(test_ntc_normal);
    TEST_RUN(test_ntc_cold);
    TEST_RUN(test_ntc_warm);
    TEST_RUN(test_ntc_critical);
    TEST_RUN(test_ntc_reduce);
    TEST_RUN(test_ntc_stop);

    TEST_RUN(test_recharge_needed);
    TEST_RUN(test_recharge_not_needed);

    TEST_RUN(test_charge_arbitrate);

    TEST_SUMMARY();
}
