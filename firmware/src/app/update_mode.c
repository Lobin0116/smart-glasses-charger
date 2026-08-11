#ifdef HIL_TEST
#include "update_mode.h"

#include <stdbool.h>
#include <stddef.h>

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
#include "button.h"
#include "gd32e23x.h"
#include "hal_i2c.h"
#include "hal_timer.h"
#include "hal_usart.h"
#include "hal_wwdgt.h"
#include "state_machine.h"

extern sm_ctx_t sm;

/* OTA debug counters, defined in ota_flow.c. Mirrored in HIL_STATUS so the PC
 * can diagnose failed transfers without a separate ASCII channel. */
extern volatile uint16_t ota_dbg_last_rx_len;
extern volatile uint16_t ota_dbg_last_index;
extern volatile uint16_t ota_dbg_parse_fails;
extern volatile uint8_t  ota_dbg_last_fail_reason;
extern volatile uint16_t ota_dbg_last_success_rx_len;

/* Send a HIL ACK frame: response magic + echoed opcode + status + payload. */
static void send_hil_ack(uint16_t opcode, at_status status,
                         const uint8_t *payload, uint8_t payload_len) {
    uint8_t buf[80];
    uint16_t len = at_frame_pack_response(buf, opcode, (uint8_t)status, payload, payload_len);
    hal_usart_send(buf, len);
}

static void handle_hil_reset(void) {
    sm.state = ST_IDLE;
    sm.retry_count = 0U;
    sm.glass_present = false;
    sm.glass_charging = false;
    sm.glass_full = false;
    sm.ota_requested = false;
    sm.reported_case_version = 0U;
    sm.lid_open = false;
    sm_inject_lid_event(false);  /* clear any pending lid event */
    send_hil_ack(AT_OPCODE_HIL_RESET, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_open(void) {
    sm_inject_lid_event(true);
    send_hil_ack(AT_OPCODE_HIL_OPEN, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_close(void) {
    sm_inject_lid_event(false);
    send_hil_ack(AT_OPCODE_HIL_CLOSE, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_key(void) {
    button_on_press();
    send_hil_ack(AT_OPCODE_HIL_KEY, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_ota(void) {
    sm.ota_requested = true;
    send_hil_ack(AT_OPCODE_HIL_OTA, AT_SUCCESS, NULL, 0U);
}

/* HIL_STATUS payload (packed, big-endian u16 fields) — 17 bytes.
 * PC unpacks with struct.unpack in sgc_at.py. */
#pragma pack(push, 1)
typedef struct {
    uint8_t ver;
    uint8_t soc_reg;
    uint8_t cfg;
    uint8_t sysclk_mhz;
    uint8_t ahb_mhz;
    uint8_t apb1_mhz;
    uint8_t case_soc;
    uint8_t state;
    uint8_t ota_index_hi;
    uint8_t ota_index_lo;
    uint8_t ota_rxlen_hi;
    uint8_t ota_rxlen_lo;
    uint8_t ota_fails_hi;
    uint8_t ota_fails_lo;
    uint8_t ota_fail_reason;
    uint8_t ota_succ_hi;
    uint8_t ota_succ_lo;
} hil_status_payload_t;
#pragma pack(pop)

static void handle_hil_status(void) {
    uint8_t ver = 0U, soc_reg = 0U, cfg = 0U;
    (void)hal_i2c_read_reg(0x63U, 0x00U, &ver, 1U);
    (void)hal_i2c_read_reg(0x63U, 0x04U, &soc_reg, 1U);
    (void)hal_i2c_read_reg(0x63U, 0x08U, &cfg, 1U);

    hil_status_payload_t p;
    p.ver = ver;
    p.soc_reg = soc_reg;
    p.cfg = cfg;
    p.sysclk_mhz = (uint8_t)(SystemCoreClock / 1000000U);
    p.ahb_mhz = (uint8_t)(rcu_clock_freq_get(CK_AHB) / 1000000U);
    p.apb1_mhz = (uint8_t)(rcu_clock_freq_get(CK_APB1) / 1000000U);
    p.case_soc = sm.case_soc;
    p.state = (uint8_t)sm.state;
    p.ota_index_hi = (uint8_t)(ota_dbg_last_index >> 8);
    p.ota_index_lo = (uint8_t)(ota_dbg_last_index & 0xFFU);
    p.ota_rxlen_hi = (uint8_t)(ota_dbg_last_rx_len >> 8);
    p.ota_rxlen_lo = (uint8_t)(ota_dbg_last_rx_len & 0xFFU);
    p.ota_fails_hi = (uint8_t)(ota_dbg_parse_fails >> 8);
    p.ota_fails_lo = (uint8_t)(ota_dbg_parse_fails & 0xFFU);
    p.ota_fail_reason = ota_dbg_last_fail_reason;
    p.ota_succ_hi = (uint8_t)(ota_dbg_last_success_rx_len >> 8);
    p.ota_succ_lo = (uint8_t)(ota_dbg_last_success_rx_len & 0xFFU);

    send_hil_ack(AT_OPCODE_HIL_STATUS, AT_SUCCESS, (const uint8_t *)&p, (uint8_t)sizeof(p));
}

static void handle_hil_scan(void) {
    uint8_t addrs[32];
    uint8_t n = 0U;
    for (uint8_t addr = 1U; addr < 0x80U && n < (uint8_t)sizeof(addrs); addr++) {
        if (hal_i2c_write(addr, NULL, 0U) == 0) {
            addrs[n++] = addr;
        }
        hal_wwdgt_feed();
    }
    send_hil_ack(AT_OPCODE_HIL_SCAN, AT_SUCCESS, addrs, n);
}

void update_mode_poll(void) {
    while (true) {
        uint8_t lead;
        if (!hal_usart_rx_peek(&lead)) {
            break;  /* ring buffer empty */
        }

        if (lead != 0x23U) {
            /* Non-magic byte — drop (garbage / mis-aligned residue). */
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        /* Peek the 10-byte header without consuming. */
        uint8_t header[AT_FRAME_HEADER_SIZE];
        if (!hal_usart_rx_peek_n(header, AT_FRAME_HEADER_SIZE)) {
            break;  /* header not fully arrived yet; wait for next poll */
        }

        uint16_t opcode = (uint16_t)(((uint16_t)header[7] << 8) | header[8]);

        /* Production protocol frames are left in the buffer for charge_poll /
         * ota_flow to consume via at_frame_recv. update_mode_poll must NOT
         * touch them — that was the root cause of the "charge_poll eats HIL
         * commands" bug. */
        if (opcode == AT_OPCODE_CASE_HEART || opcode == AT_OPCODE_CASE_SHUTDOWN ||
            opcode == AT_OPCODE_CASE_PACKET_PREPARE || opcode == AT_OPCODE_CASE_PACKET_READ) {
            break;
        }

        /* Only HIL opcodes are ours. Anything else: drop the lead byte. */
        if (opcode < AT_OPCODE_HIL_RESET || opcode > AT_OPCODE_HIL_OTA) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        uint16_t size = (uint16_t)(((uint16_t)header[5] << 8) | header[6]);
        if (size < AT_FRAME_HEADER_SIZE || size > 64U) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        /* Wait for the full frame to arrive, then consume it. Header already
         * in the buffer; payload follows within (size-10) × 87us at 115200. */
        uint8_t frame[64];
        uint32_t wait_start = hal_timer_get_ms();
        while (!hal_usart_rx_peek_n(frame, size)) {
            if (hal_timer_expired(wait_start, 50U)) {
                /* Timeout — drop what we have and let the caller retry. */
                uint8_t tmp;
                (void)hal_usart_rx_get(&tmp);
                goto next;
            }
        }
        for (uint16_t i = 0U; i < size; i++) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
        }

        switch (opcode) {
        case AT_OPCODE_HIL_RESET:  handle_hil_reset();  break;
        case AT_OPCODE_HIL_OPEN:   handle_hil_open();   break;
        case AT_OPCODE_HIL_CLOSE:  handle_hil_close();  break;
        case AT_OPCODE_HIL_KEY:    handle_hil_key();    break;
        case AT_OPCODE_HIL_STATUS: handle_hil_status(); break;
        case AT_OPCODE_HIL_SCAN:   handle_hil_scan();   break;
        case AT_OPCODE_HIL_OTA:    handle_hil_ota();    break;
        default: break;
        }

    next:
        continue;
    }
    hal_wwdgt_feed();
}
#endif
