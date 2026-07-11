#include "mock_spl.h"
#include <string.h>

/* Stubs for hardware-dependent functions called by charge_flow.c and aux_logic.c */

void led_all_off(void) {}
void led_set(int color, int mode) { (void)color; (void)mode; }
void mt5706_disable(void) {}
void mt5706_enable(void) {}

void hal_pwr_pulse_charge(uint32_t ms) { (void)ms; }
void hal_pwr_discharge(uint32_t ms) { (void)ms; }
void hal_pwr_enter_comm(void) {}
void hal_pwr_enter_charge(void) {}
void hal_pwr_idle(void) {}

void hal_timer_delay_ms(uint32_t ms) { (void)ms; }
uint32_t hal_timer_get_ms(void) { return 0; }
uint32_t hal_timer_elapsed(uint32_t start) { return 0; }
bool hal_timer_expired(uint32_t start, uint32_t timeout) {
    (void)start;
    (void)timeout;
    return false;
}

uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max,
                             uint32_t timeout_ms) {
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_max;
    (void)timeout_ms;
    return 0;
}

uint16_t at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload,
                                uint8_t payload_len, uint8_t reserved) {
    (void)buf;
    (void)opcode;
    (void)payload;
    (void)payload_len;
    (void)reserved;
    return 0;
}

int at_frame_parse(const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status,
                   uint8_t *payload, uint8_t *payload_len) {
    (void)buf;
    (void)total_len;
    (void)opcode;
    (void)status;
    (void)payload;
    (void)payload_len;
    return 0;
}
