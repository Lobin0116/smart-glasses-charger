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
    #include "hal_gpio.h"
    #include "hal_timer.h"
    #include "hal_usart.h"
    #include "hal_wwdgt.h"
    #include "nu1671.h"
    #include "state_machine.h"

extern sm_ctx_t sm;

extern volatile uint16_t ota_dbg_last_rx_len;
extern volatile uint16_t ota_dbg_last_index;
extern volatile uint16_t ota_dbg_parse_fails;
extern volatile uint8_t ota_dbg_last_fail_reason;
extern volatile uint16_t ota_dbg_last_success_rx_len;
extern volatile uint8_t at_frame_last_fail_stage;
extern volatile uint16_t at_frame_last_buf_bytes;

static void send_hil_ack(uint16_t opcode, at_status status, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t buf[80];
    uint16_t len = at_frame_pack_response(buf, opcode, (uint8_t)status, payload, payload_len);
    hal_usart_send(buf, len);
}

static void handle_hil_reset(void)
{

    hal_usart_rx_clear();
    sm.state = ST_IDLE;
    sm.retry_count = 0U;
    sm.glass_present = false;
    sm.glass_charging = false;
    sm.glass_full = false;
    sm.ota_requested = false;
    sm.reported_case_version = 0U;
    sm.lid_open = false;
    hal_hall_set_mock(false);
    send_hil_ack(AT_OPCODE_HIL_RESET, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_open(void)
{

    hal_hall_set_mock(true);
    send_hil_ack(AT_OPCODE_HIL_OPEN, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_close(void)
{
    hal_hall_set_mock(false);
    send_hil_ack(AT_OPCODE_HIL_CLOSE, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_key(void)
{
    button_on_press();
    send_hil_ack(AT_OPCODE_HIL_KEY, AT_SUCCESS, NULL, 0U);
}

static void handle_hil_ota(void)
{
    sm.ota_requested = true;
    send_hil_ack(AT_OPCODE_HIL_OTA, AT_SUCCESS, NULL, 0U);
}

    #pragma pack(push, 1)
typedef struct
{
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
    uint8_t at_frame_fail_stage;
    uint8_t at_frame_buf_bytes_hi;
    uint8_t at_frame_buf_bytes_lo;
} hil_status_payload_t;
    #pragma pack(pop)

static void handle_hil_status(void)
{
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
    p.at_frame_fail_stage = at_frame_last_fail_stage;
    p.at_frame_buf_bytes_hi = (uint8_t)(at_frame_last_buf_bytes >> 8);
    p.at_frame_buf_bytes_lo = (uint8_t)(at_frame_last_buf_bytes & 0xFFU);

    send_hil_ack(AT_OPCODE_HIL_STATUS, AT_SUCCESS, (const uint8_t *)&p, (uint8_t)sizeof(p));
}

static void handle_hil_scan(void)
{
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
static void handle_hil_chg_diag(void)
{
    uint8_t ip_state0 = 0xFFU, ip_state2 = 0xFFU, ip_state5 = 0xFFU;
    uint8_t ip_ntc = 0xFFU;
    uint8_t cw_vcell[2] = {0xFFU, 0xFFU};
    uint8_t cw_soc = 0xFFU, cw_ver = 0xFFU, cw_cfg = 0xFFU;
    uint8_t ok = 0U;

    if (hal_i2c_read_reg(0x75U, 0x45U, &ip_state0, 1U) == 0) {
        ok |= 0x01U;
    }
    if (hal_i2c_read_reg(0x75U, 0x50U, &ip_state2, 1U) == 0) {
        ok |= 0x02U;
    }
    if (hal_i2c_read_reg(0x75U, 0x69U, &ip_state5, 1U) == 0) {
        ok |= 0x04U;
    }
    if (hal_i2c_read_reg(0x75U, 0x6FU, &ip_ntc, 1U) == 0) {
        ok |= 0x80U;
    }
    if (hal_i2c_read_reg(0x63U, 0x02U, cw_vcell, 2U) == 0) {
        ok |= 0x08U;
    }
    if (hal_i2c_read_reg(0x63U, 0x04U, &cw_soc, 1U) == 0) {
        ok |= 0x10U;
    }
    if (hal_i2c_read_reg(0x63U, 0x00U, &cw_ver, 1U) == 0) {
        ok |= 0x20U;
    }
    if (hal_i2c_read_reg(0x63U, 0x08U, &cw_cfg, 1U) == 0) {
        ok |= 0x40U;
    }
    hal_wwdgt_feed();

    uint8_t flags = 0U;
    if (hal_charger_int_get()) {
        flags |= 0x01U;
    }
    if (hal_nint_get()) {
        flags |= 0x02U;
    }
    if (hal_power_gate_is_on(HAL_POWER_GATE_POGO3V3)) {
        flags |= 0x04U;
    }
    if (!hal_pdetb_get()) {
        flags |= 0x08U;
    }
    nu1671_probe();
    if (nu1671_is_present()) {
        flags |= 0x10U;
    }

    uint8_t p[11] = {0U};
    p[0] = flags;
    p[1] = ok;
    p[2] = ip_state0;
    p[3] = ip_state2;
    p[4] = ip_state5;
    p[5] = cw_vcell[0];
    p[6] = cw_vcell[1];
    p[7] = cw_soc;
    p[8] = cw_ver;
    p[9] = cw_cfg;
    p[10] = ip_ntc;
    send_hil_ack(AT_OPCODE_HIL_CHG_DIAG, AT_SUCCESS, p, (uint8_t)sizeof(p));
}

void update_mode_poll(void)
{
    while (true) {
        uint8_t lead;
        if (!hal_usart_rx_peek(&lead)) {
            break;
        }

        if (lead != 0x23U) {

            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        uint8_t header[AT_FRAME_HEADER_SIZE];
        uint8_t frame_buf[64];
        if (!hal_usart_rx_peek_n(header, AT_FRAME_HEADER_SIZE)) {
            break;
        }

        uint16_t opcode = (uint16_t)((uint16_t)header[7] | ((uint16_t)header[8] << 8));

        if (opcode == AT_OPCODE_CASE_HEART || opcode == AT_OPCODE_CASE_SHUTDOWN
            || opcode == AT_OPCODE_CASE_PACKET_PREPARE || opcode == AT_OPCODE_CASE_PACKET_READ) {
            break;
        }

        if (opcode < AT_OPCODE_HIL_RESET || opcode > AT_OPCODE_HIL_CHG_DIAG) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        uint16_t size = (uint16_t)((uint16_t)header[5] | ((uint16_t)header[6] << 8));
        uint16_t total_len = (uint16_t)(AT_FRAME_HEADER_SIZE + size);
        if (total_len > (uint16_t)sizeof(frame_buf)) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
            continue;
        }

        uint32_t wait_start = hal_timer_get_ms();
        while (!hal_usart_rx_peek_n(frame_buf, total_len)) {
            if (hal_timer_expired(wait_start, 50U)) {

                uint8_t tmp;
                (void)hal_usart_rx_get(&tmp);
                goto next;
            }
        }
        for (uint16_t i = 0U; i < total_len; i++) {
            uint8_t tmp;
            (void)hal_usart_rx_get(&tmp);
        }

        switch (opcode) {
            case AT_OPCODE_HIL_RESET:
                handle_hil_reset();
                break;
            case AT_OPCODE_HIL_OPEN:
                handle_hil_open();
                break;
            case AT_OPCODE_HIL_CLOSE:
                handle_hil_close();
                break;
            case AT_OPCODE_HIL_KEY:
                handle_hil_key();
                break;
            case AT_OPCODE_HIL_STATUS:
                handle_hil_status();
                break;
            case AT_OPCODE_HIL_SCAN:
                handle_hil_scan();
                break;
            case AT_OPCODE_HIL_CHG_DIAG:
                handle_hil_chg_diag();
                break;
            case AT_OPCODE_HIL_OTA:
                handle_hil_ota();
                break;
            default:
                break;
        }

    next:
        continue;
    }
    hal_wwdgt_feed();
}
#endif
