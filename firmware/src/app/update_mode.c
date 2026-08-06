#ifdef HIL_TEST
#include "update_mode.h"

#include <stdbool.h>
#include <stddef.h>

#include "button.h"
#include "gd32e23x.h"
#include "hal_i2c.h"
#include "hal_usart.h"
#include "hal_wwdgt.h"
#include "state_machine.h"

extern sm_ctx_t sm;

/* OTA debug counters, defined in ota_flow.c. Exposed via STATUS as
 * "ODiXXXXlXXXXfXXXXrXX" (index, rx_len, parse_fails, fail_reason). */
extern volatile uint16_t ota_dbg_last_rx_len;
extern volatile uint16_t ota_dbg_last_index;
extern volatile uint16_t ota_dbg_parse_fails;
extern volatile uint8_t  ota_dbg_last_fail_reason;
extern volatile uint16_t ota_dbg_last_success_rx_len;

#define CMD_BUF_SIZE 16U

static bool str_eq(const char *a, const char *b, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void send_ack(const uint8_t *s, uint16_t len) {
    hal_usart_send(s, len);
}

static void emit_hex(uint8_t v, uint8_t *buf, uint8_t *idx) {
    uint8_t hi = (uint8_t)((v >> 4) & 0x0FU);
    uint8_t lo = (uint8_t)(v & 0x0FU);
    buf[(*idx)++] = (uint8_t)(hi < 10U ? ('0' + hi) : ('A' + hi - 10U));
    buf[(*idx)++] = (uint8_t)(lo < 10U ? ('0' + lo) : ('A' + lo - 10U));
}

void update_mode_poll(void) {
    static char buf[CMD_BUF_SIZE];
    static uint8_t idx = 0;
    uint8_t c;

    while (hal_usart_rx_peek(&c)) {
        if (c == '\n' || c == '\r') {
            (void)hal_usart_rx_get(&c);
            static const uint8_t ok_open[] = "OK_OPEN\n";
            static const uint8_t ok_close[] = "OK_CLOSE\n";
            static const uint8_t ok_key[] = "OK_KEY\n";
            if (idx == 4U && str_eq(buf, "OPEN", 4U)) {
                send_ack(ok_open, 8U);
                sm_inject_lid_event(true);
            } else if (idx == 5U && str_eq(buf, "CLOSE", 5U)) {
                send_ack(ok_close, 9U);
                sm_inject_lid_event(false);
            } else if (idx == 3U && str_eq(buf, "KEY", 3U)) {
                send_ack(ok_key, 7U);
                button_on_press();
            } else if (idx == 3U && str_eq(buf, "OTA", 3U)) {
                static const uint8_t ok_ota[] = "OK_OTA\n";
                send_ack(ok_ota, 7U);
                sm.ota_requested = true;
            } else if (idx == 5U && str_eq(buf, "RESET", 5U)) {
                static const uint8_t ok_reset[] = "OK_RESET\n";
                send_ack(ok_reset, 9U);
                sm.state = ST_IDLE;
                sm.retry_count = 0U;
                sm.glass_present = false;
                sm.glass_charging = false;
                sm.glass_full = false;
                sm.ota_requested = false;
                sm.reported_case_version = 0U;
                sm.lid_open = false;
                sm_inject_lid_event(false);  /* clear any pending lid event */
            } else if (idx == 6U && str_eq(buf, "STATUS", 6U)) {
                uint8_t msg[80];
                uint8_t n = 0U;
                uint8_t ver = 0U, soc = 0U, cfg = 0U;
                int ok_v = hal_i2c_read_reg(0x63U, 0x00U, &ver, 1U);
                int ok_s = hal_i2c_read_reg(0x63U, 0x04U, &soc, 1U);
                int ok_c = hal_i2c_read_reg(0x63U, 0x08U, &cfg, 1U);
                msg[n++] = 'V'; msg[n++] = '=';
                emit_hex(ver, msg, &n);
                msg[n++] = (ok_v == 0) ? ' ' : '!';
                msg[n++] = 'S'; msg[n++] = '=';
                emit_hex(soc, msg, &n);
                msg[n++] = (ok_s == 0) ? ' ' : '!';
                msg[n++] = 'C'; msg[n++] = '=';
                emit_hex(cfg, msg, &n);
                msg[n++] = (ok_c == 0) ? ' ' : '!';
                msg[n++] = ' ';
                msg[n++] = 'F'; msg[n++] = '=';
                emit_hex((uint8_t)(SystemCoreClock / 1000000U), msg, &n);
                msg[n++] = ' ';
                msg[n++] = 'H'; msg[n++] = '=';
                emit_hex((uint8_t)(rcu_clock_freq_get(CK_AHB) / 1000000U), msg, &n);
                msg[n++] = ' ';
                msg[n++] = 'P'; msg[n++] = '=';
                emit_hex((uint8_t)(rcu_clock_freq_get(CK_APB1) / 1000000U), msg, &n);
                msg[n++] = ' ';
                msg[n++] = 'O'; msg[n++] = 'D';
                msg[n++] = 'i'; emit_hex((uint8_t)(ota_dbg_last_index >> 8), msg, &n);
                emit_hex((uint8_t)(ota_dbg_last_index & 0xFFU), msg, &n);
                msg[n++] = 'l'; emit_hex((uint8_t)(ota_dbg_last_rx_len >> 8), msg, &n);
                emit_hex((uint8_t)(ota_dbg_last_rx_len & 0xFFU), msg, &n);
                msg[n++] = 'f'; emit_hex((uint8_t)(ota_dbg_parse_fails >> 8), msg, &n);
                emit_hex((uint8_t)(ota_dbg_parse_fails & 0xFFU), msg, &n);
                msg[n++] = 'r'; emit_hex(ota_dbg_last_fail_reason, msg, &n);
                msg[n++] = 's'; emit_hex((uint8_t)(ota_dbg_last_success_rx_len >> 8), msg, &n);
                emit_hex((uint8_t)(ota_dbg_last_success_rx_len & 0xFFU), msg, &n);
                msg[n++] = '\n';
                send_ack(msg, n);
            } else if (idx == 4U && str_eq(buf, "SCAN", 4U)) {
                uint8_t msg[160];
                uint8_t n = 0U;
                msg[n++] = 'I'; msg[n++] = '2'; msg[n++] = 'C'; msg[n++] = ':';
                for (uint8_t addr = 1U; addr < 0x80U; addr++) {
                    if (hal_i2c_write(addr, NULL, 0U) == 0) {
                        emit_hex(addr, msg, &n);
                        msg[n++] = ' ';
                    }
                    hal_wwdgt_feed();
                }
                msg[n++] = '\n';
                send_ack(msg, n);
            } else if (idx == 3U && str_eq(buf, "I2C", 3U)) {
                static const uint8_t ok[] = "I2C START\n";
                send_ack(ok, 9U);
                while (1) {
                    i2c_start_on_bus(I2C0);
                    for (volatile uint32_t d = 0U; d < 200U; d++) {
                    }
                    i2c_master_addressing(I2C0, (uint32_t)0x63U << 1U, I2C_TRANSMITTER);
                    for (volatile uint32_t d = 0U; d < 200U; d++) {
                    }
                    i2c_stop_on_bus(I2C0);
                    for (volatile uint32_t d = 0U; d < 5000U; d++) {
                    }
                    hal_wwdgt_feed();
                }
            }
            idx = 0U;
        } else if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && idx < CMD_BUF_SIZE) {
            (void)hal_usart_rx_get(&c);
            buf[idx++] = (char)c;
        } else {
            /* Drop non-command byte (e.g. residue from a previous response) so
             * it doesn't block parsing of the next command. */
            (void)hal_usart_rx_get(&c);
        }
    }
    hal_wwdgt_feed();
}
#endif
