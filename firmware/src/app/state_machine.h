#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

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
    uint32_t state_enter_ms; /* timestamp when entered current state */
    uint32_t last_comms_ms;  /* last successful heartbeat            */
    uint8_t retry_count;     /* handshake/charge retry counter       */
    bool lid_open;           /* hall sensor state                    */
    bool glass_present;      /* glasses detected via heartbeat       */
    uint8_t glass_soc;       /* last known glasses SOC               */
    uint8_t case_soc;        /* last known case SOC                  */
    bool glass_charging;     /* glasses being charged                */
    bool glass_full;         /* glasses reported full                */
    bool ota_requested;      /* OTA flag from heartbeat              */
} sm_ctx_t;

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
