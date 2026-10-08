#ifndef CHARGE_FLOW_H
#define CHARGE_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "at_types.h"
#include "state_machine.h"

#define HANDSHAKE_5V_MS             300U
#define HANDSHAKE_DISCHARGE_MS      100U
#define HANDSHAKE_RETRY_COUNT       3U
#define HANDSHAKE_RETRY_INTERVAL_MS 100U
#define CHARGE_POLL_OPEN_MS         30000U
#define CHARGE_POLL_CLOSED_MS       60000U
#define MAINTAIN_HEARTBEAT_MS       1000U
#define FORCE_CHARGE_INTERVAL_MS    180000U
#define FORCE_CHARGE_DURATION_MS    540000U
#define SHUTDOWN_RETRY_COUNT        5U
#define COMM_TIMEOUT_MS             100U
#define RECHARGE_THRESHOLD          98U

/* Non-blocking POGO transaction engine (charge_flow.c). One transaction =
 * [optional 5V pulse] -> 100 ms bus discharge -> UART switch -> heartbeat
 * request(s) with COMM_TIMEOUT_MS reply window and HANDSHAKE_RETRY_INTERVAL_MS
 * retry gaps -> rail restored to charge. Each pogo_tick() call advances at
 * most one phase and never blocks, so the main loop stays responsive while an
 * exchange is in flight.
 *
 * Usage pattern from the state machine: pogo_start(...) once, then pogo_tick()
 * every sm_tick while pogo_busy(); when busy goes false, pogo_done_ok() tells
 * whether any heartbeat succeeded and pogo_reply() (valid only then) carries
 * the glasses status. The engine never writes sm_ctx — glass_present/
 * glass_soc/glass_full/last_comms updates belong to the caller, exactly as
 * they did with the old blocking flows. */

/* Begin a transaction: pulse=true drives the 5V charge rail for
 * HANDSHAKE_5V_MS first (handshake / force probe); hb_retries bounds the
 * heartbeat attempts (HANDSHAKE_RETRY_COUNT for handshake, 1 for polls). */
void pogo_start(sm_ctx_t *ctx, bool pulse, uint8_t hb_retries);

/* Advance the in-flight transaction by at most one phase. Never blocks; the
 * single longest action is the ~1.2 ms heartbeat send. */
void pogo_tick(sm_ctx_t *ctx);

/* True from pogo_start() until the rail-restore phase completes. */
bool pogo_busy(void);

/* Result of the last completed transaction: true when at least one heartbeat
 * exchange succeeded. Read only once pogo_busy() has gone false. */
bool pogo_done_ok(void);

/* Glasses status from the successful heartbeat reply; valid only when
 * pogo_done_ok() is true. */
const at_glass_data *pogo_reply(void);

/* Deliver the shutdown command (rare, ~100 ms, deliberately still blocking). */
bool sm_do_shutdown(void);

uint8_t sm_build_case_soc_byte(uint8_t soc, bool charging);
uint8_t sm_build_case_sta_byte(bool lid_open, bool ota_requested);

#endif
