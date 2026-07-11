/*
 * Unit tests for state machine transition paths.
 *
 * We mock hal_timer to advance time, and mock charge_flow/ota functions
 * to control handshake/poll/shutdown success/failure.
 * This verifies every major transition path described in CONTEXT.md.
 */

#include "led.h"
#include "led_effect.h"
#include "ota_flow.h"
#include "state_machine.h"
#include "test_assert.h"

/* --- mock timer --- */
static uint32_t mock_time_ms;
uint32_t hal_timer_get_ms(void) { return mock_time_ms; }
uint32_t hal_timer_elapsed(uint32_t start) { return mock_time_ms - start; }
bool hal_timer_expired(uint32_t start, uint32_t timeout) {
    return (mock_time_ms - start) >= timeout;
}
void hal_timer_delay_ms(uint32_t ms) { mock_time_ms += ms; }

/* --- mock charge_flow results --- */
static bool mock_handshake_result;
static bool mock_charge_poll_result;
static bool mock_maintain_result;
static bool mock_force_probe_result;
static int mock_shutdown_calls;
static int mock_handshake_calls;

bool sm_do_handshake(sm_ctx_t *ctx) {
    (void)ctx;
    mock_handshake_calls++;
    return mock_handshake_result;
}
bool sm_do_charge_poll(sm_ctx_t *ctx) {
    (void)ctx;
    return mock_charge_poll_result;
}
bool sm_do_maintain_heartbeat(sm_ctx_t *ctx) {
    (void)ctx;
    return mock_maintain_result;
}
bool sm_do_force_charge_probe(sm_ctx_t *ctx) {
    (void)ctx;
    return mock_force_probe_result;
}
bool sm_do_shutdown(void) {
    mock_shutdown_calls++;
    return true;
}

/* stubs for other deps */
void pm_enter_deep_sleep(void) {}
void hal_pwr_idle(void) {}
void led_effect_show_battery(led_effect_ctx_t *ctx, uint8_t soc) {
    (void)ctx;
    (void)soc;
}
int ota_run(sm_ctx_t *ctx, ota_progress_cb_t cb) {
    (void)ctx;
    (void)cb;
    return 0;
}
void ota_init(void) {}
int8_t cw2017_get_temp_c(void) { return 25; }
void mt5706_disable(void) {}
void mt5706_enable(void) {}
bool mt5706_has_event(void) { return false; }
void led_all_off(void) {}
void led_set(led_color_t color, led_mode_t mode) {
    (void)color;
    (void)mode;
}

led_effect_ctx_t g_led_ctx;
static bool mock_hall_state;
bool hal_hall_get(void) { return mock_hall_state; }

/* --- helpers --- */
static void reset_sm(sm_ctx_t *ctx) {
    mock_time_ms = 0;
    mock_handshake_result = false;
    mock_charge_poll_result = false;
    mock_maintain_result = false;
    mock_force_probe_result = false;
    mock_shutdown_calls = 0;
    mock_handshake_calls = 0;
    mock_hall_state = false;
    sm_init(ctx);
}

static void advance_time(sm_ctx_t *ctx, uint32_t ms) {
    mock_time_ms += ms;
    sm_tick(ctx);
}

static void set_state(sm_ctx_t *ctx, sm_state_t s) {
    ctx->state = s;
    ctx->state_enter_ms = mock_time_ms;
    ctx->retry_count = 0;
}
#define ATTEMPT_GAP 600U
#define TIMEOUT_MS 30000U

/* === tests === */

static void test_init_state(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    TEST_ASSERT_EQ(ctx.state, ST_IDLE, "Init should be IDLE");
    TEST_ASSERT(!ctx.lid_open, "Lid should be closed");
    TEST_ASSERT(!ctx.glass_present, "No glass present");
}

static void test_lid_open_starts_handshake(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    mock_hall_state = true;
    sm_handle_event(&ctx, 4);
    sm_tick(&ctx); /* deferred processing in main loop */
    TEST_ASSERT(ctx.lid_open, "Lid should be open after HALL event");
    TEST_ASSERT_EQ(ctx.state, ST_HANDSHAKING, "Should enter handshaking");
}

static void test_handshake_success_high_soc_to_charging(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    ctx.lid_open = true;
    set_state(&ctx, ST_HANDSHAKING);
    mock_handshake_result = true;
    advance_time(&ctx, ATTEMPT_GAP);
    TEST_ASSERT_EQ(ctx.state, ST_CHARGING, "High SOC + handshake success -> CHARGING");
    TEST_ASSERT(ctx.glass_present, "Glass should be present");
}

static void test_handshake_success_low_soc_to_maintaining(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 10;
    ctx.lid_open = true;
    set_state(&ctx, ST_HANDSHAKING);
    mock_handshake_result = true;
    advance_time(&ctx, ATTEMPT_GAP);
    TEST_ASSERT_EQ(ctx.state, ST_MAINTAINING, "Low SOC + success -> MAINTAINING");
}

static void test_handshake_fail_high_soc_to_force_charging(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    ctx.lid_open = true;
    set_state(&ctx, ST_HANDSHAKING);
    mock_handshake_result = false;
    advance_time(&ctx, TIMEOUT_MS + ATTEMPT_GAP);
    TEST_ASSERT_EQ(ctx.state, ST_FORCE_CHARGING, "High SOC + fail timeout -> FORCE");
}

static void test_handshake_fail_low_soc_to_idle(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 10;
    ctx.lid_open = true;
    set_state(&ctx, ST_HANDSHAKING);
    mock_handshake_result = false;
    advance_time(&ctx, TIMEOUT_MS + ATTEMPT_GAP);
    TEST_ASSERT_EQ(ctx.state, ST_IDLE, "Low SOC + fail timeout -> IDLE");
}

static void test_charging_glass_full_lid_open_to_maintaining(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    ctx.lid_open = true;
    ctx.glass_full = true;
    set_state(&ctx, ST_CHARGING);
    sm_tick(&ctx);
    TEST_ASSERT_EQ(ctx.state, ST_MAINTAINING, "Glass full + lid open -> MAINTAINING");
}

static void test_charging_glass_full_lid_closed_to_shutting_down(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    ctx.lid_open = false;
    ctx.glass_full = true;
    set_state(&ctx, ST_CHARGING);
    sm_tick(&ctx);
    TEST_ASSERT_EQ(ctx.state, ST_SHUTTING_DOWN, "Glass full + lid closed -> SHUTDOWN");
}

static void test_charging_ota_request_to_ota(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    ctx.ota_requested = true;
    set_state(&ctx, ST_CHARGING);
    sm_tick(&ctx);
    TEST_ASSERT_EQ(ctx.state, ST_OTA, "OTA request -> OTA state");
}

static void test_force_charging_success_to_charging(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    set_state(&ctx, ST_FORCE_CHARGING);
    mock_force_probe_result = true;
    advance_time(&ctx, 180001); /* FORCE_PROBE_GAP */
    TEST_ASSERT_EQ(ctx.state, ST_CHARGING, "Force probe success -> CHARGING");
    TEST_ASSERT(ctx.glass_present, "Glass should be present");
}

static void test_force_charging_timeout_to_idle(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    set_state(&ctx, ST_FORCE_CHARGING);
    mock_force_probe_result = false;
    advance_time(&ctx, 540001); /* FORCE_TIMEOUT */
    TEST_ASSERT_EQ(ctx.state, ST_IDLE, "Force timeout -> IDLE");
}

static void test_shutdown_retries_then_idle(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 80;
    set_state(&ctx, ST_SHUTTING_DOWN);
    for (int i = 0; i < 6; i++) {
        advance_time(&ctx, 100);
    }
    TEST_ASSERT_EQ(ctx.state, ST_IDLE, "After 5 shutdown retries -> IDLE");
    TEST_ASSERT_EQ(mock_shutdown_calls, 5, "Should have called shutdown 5 times");
}

static void test_low_batt_idle_sends_shutdown(void) {
    sm_ctx_t ctx;
    reset_sm(&ctx);
    ctx.case_soc = 10;
    ctx.glass_present = true;
    ctx.lid_open = true;
    set_state(&ctx, ST_HANDSHAKING);
    mock_handshake_result = false;
    advance_time(&ctx, TIMEOUT_MS + ATTEMPT_GAP);
    TEST_ASSERT_EQ(ctx.state, ST_IDLE, "Should be IDLE");
    TEST_ASSERT(mock_shutdown_calls > 0, "Should have sent shutdown to glasses");
}

int main(void) {
    printf("=== State Machine Transition Tests ===\n\n");
    TEST_RUN(test_init_state);
    TEST_RUN(test_lid_open_starts_handshake);
    TEST_RUN(test_handshake_success_high_soc_to_charging);
    TEST_RUN(test_handshake_success_low_soc_to_maintaining);
    TEST_RUN(test_handshake_fail_high_soc_to_force_charging);
    TEST_RUN(test_handshake_fail_low_soc_to_idle);
    TEST_RUN(test_charging_glass_full_lid_open_to_maintaining);
    TEST_RUN(test_charging_glass_full_lid_closed_to_shutting_down);
    TEST_RUN(test_charging_ota_request_to_ota);
    TEST_RUN(test_force_charging_success_to_charging);
    TEST_RUN(test_force_charging_timeout_to_idle);
    TEST_RUN(test_shutdown_retries_then_idle);
    TEST_RUN(test_low_batt_idle_sends_shutdown);
    TEST_SUMMARY();
}
