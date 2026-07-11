#ifndef CHARGE_FLOW_H
#define CHARGE_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "state_machine.h"

#define HANDSHAKE_5V_MS 300U
#define HANDSHAKE_DISCHARGE_MS 100U
#define HANDSHAKE_RETRY_COUNT 3U
#define HANDSHAKE_RETRY_INTERVAL_MS 100U
#define CHARGE_POLL_OPEN_MS 30000U
#define CHARGE_POLL_CLOSED_MS 60000U
#define MAINTAIN_HEARTBEAT_MS 1000U
#define FORCE_CHARGE_INTERVAL_MS 180000U
#define FORCE_CHARGE_DURATION_MS 540000U
#define SHUTDOWN_RETRY_COUNT 5U
#define COMM_TIMEOUT_MS 100U
#define RECHARGE_THRESHOLD 98U

bool sm_do_handshake(sm_ctx_t *ctx);
bool sm_do_charge_poll(sm_ctx_t *ctx);
bool sm_do_maintain_heartbeat(sm_ctx_t *ctx);
bool sm_do_force_charge_probe(sm_ctx_t *ctx);
bool sm_do_shutdown(void);

uint8_t sm_build_case_soc_byte(uint8_t soc, bool charging);
uint8_t sm_build_case_sta_byte(bool lid_open, bool ota_requested);

#endif
