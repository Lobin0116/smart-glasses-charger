#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "led_effect.h"

/* Application run state machine. The charger case is a bare-metal super-loop,
 * so every state is a small non-blocking step driven from sm_tick(). The lid
 * level is polled at the top of each sm_tick by reading hal_hall_get() — EXTI
 * is wake-only, no event queue. Slow periodic work (heartbeat polling,
 * retries) also happens in sm_tick(). */

typedef enum
{
    ST_IDLE,           /* lid shut, no glass, low-power standby              */
    ST_HANDSHAKING,    /* 5V pulse -> discharge -> heartbeat, retry x3       */
    ST_CHARGING,       /* glass present and on the 5V rail, heartbeat polled */
    ST_MAINTAINING,    /* glass present but case too low to charge           */
    ST_FORCE_CHARGING, /* blind 5V after handshake failure, probe for glass  */
    ST_SHUTTING_DOWN,  /* delivering the shutdown command, then sleep        */
    ST_OTA,            /* firmware transfer in progress; other work blocked  */
    ST_SHIP_MODE,      /* standby, NRST-only wake                            */
} sm_state_t;

typedef struct
{
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
    uint8_t reported_case_version; /* mirrored by glasses via at_glass_data.case_version */
    uint32_t last_soc_refresh_ms;
    /* Last CW2017 temperature sample, refreshed by main's 500 ms poll
     * (refresh_case_status). The state machine must not read the gauge on
     * every tick — that is an I2C transaction per main-loop pass. */
    int8_t ntc_temp_c;
    /* Set by the HALL EXTI ISR (any edge), cleared by sm_tick once it has
     * re-sampled the level. Decouples "an edge happened" from "the level
     * changed": a close+open contained inside one ~1.1 s handshake burst
     * leaves the pin level unchanged but the user clearly actuated the lid,
     * so we must still run the open path. */
    volatile bool hall_edge_seen;
} sm_ctx_t;

extern led_effect_ctx_t g_led_ctx;

/* Reset ctx to ST_IDLE and seed the timestamps. Run once before sm_tick(). */
void sm_init(sm_ctx_t *ctx);

/* Advance the machine one step. Call from the main loop. */
void sm_tick(sm_ctx_t *ctx);

/* True only when the machine is in ST_IDLE and nothing else needs the main loop
 * (no LED overlay, no charging breath, no button debounce). The caller uses
 * this to decide whether to enter Deep-Sleep. */
bool sm_can_sleep(const sm_ctx_t *ctx);

/* State name for debug logging. */
const char *sm_state_name(sm_state_t state);

#endif /* STATE_MACHINE_H */
