#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "led_effect.h"

/* Application run state machine. The charger case is a bare-metal super-loop,
 * so every state is a small non-blocking step driven from sm_tick(). External
 * edges (lid open, charge IRQs) reach the machine through sm_handle_event(); the
 * slow periodic work (heartbeat polling, retries) happens in sm_tick(). */

typedef enum {
    ST_IDLE,           /* lid shut, no glass, low-power standby              */
    ST_HANDSHAKING,    /* 5V pulse -> discharge -> heartbeat, retry x3       */
    ST_CHARGING,       /* glass present and on the 5V rail, heartbeat polled */
    ST_MAINTAINING,    /* glass present but case too low to charge           */
    ST_FORCE_CHARGING, /* blind 5V after handshake failure, probe for glass  */
    ST_SHUTTING_DOWN,  /* delivering the shutdown command, then sleep        */
    ST_OTA,            /* firmware transfer in progress; other work blocked  */
    ST_SHIP_MODE,      /* standby, NRST-only wake                            */
} sm_state_t;

typedef struct {
    sm_state_t state;
    uint32_t state_enter_ms;
    uint32_t last_comms_ms;
    uint8_t retry_count;
    bool lid_open;
    bool glass_present;
    uint8_t glass_soc;
    uint8_t case_soc;
    bool glass_charging;
    bool glass_full;
    bool ota_requested;
    uint32_t last_soc_refresh_ms;
} sm_ctx_t;

extern led_effect_ctx_t g_led_ctx;

/* Reset ctx to ST_IDLE and seed the timestamps. Run once before sm_tick(). */
void sm_init(sm_ctx_t *ctx);

/* Advance the machine one step. Call from the main loop. */
void sm_tick(sm_ctx_t *ctx);

/* Dispatch an EXTI edge. Called from the EXTI ISR callback; transitions that
 * depend on the edge (lid open out of IDLE) run here, while the heavy periodic
 * work stays in sm_tick(). */
void sm_handle_event(sm_ctx_t *ctx, uint8_t exti_line);

/* State name for debug logging. */
const char *sm_state_name(sm_state_t state);

#endif /* STATE_MACHINE_H */
