#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "led_effect.h"

typedef enum
{
    ST_IDLE,
    ST_HANDSHAKING,
    ST_CHARGING,
    ST_MAINTAINING,
    ST_FORCE_CHARGING,
    ST_SHUTTING_DOWN,
    ST_OTA,
    ST_SHIP_MODE,
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
    uint8_t reported_case_version;
    uint32_t last_soc_refresh_ms;

    int8_t ntc_temp_c;

    volatile bool hall_edge_seen;
} sm_ctx_t;

extern led_effect_ctx_t g_led_ctx;

void sm_init(sm_ctx_t *ctx);

void sm_tick(sm_ctx_t *ctx);

bool sm_can_sleep(const sm_ctx_t *ctx);

const char *sm_state_name(sm_state_t state);

#endif
