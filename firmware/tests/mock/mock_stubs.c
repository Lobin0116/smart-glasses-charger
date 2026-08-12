#include "mock_spl.h"
#include <string.h>

/* Stubs for hardware-dependent functions called by charge_flow.c and aux_logic.c */

void led_all_off(void) {}
void led_set(int color, int mode)
{
    (void)color;
    (void)mode;
}
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
bool hal_timer_expired(uint32_t start, uint32_t timeout)
{
    (void)start;
    (void)timeout;
    return false;
}

uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max, uint32_t timeout_ms)
{
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_max;
    (void)timeout_ms;
    return 0;
}

/* charge_flow.c uses hal_usart_send + at_frame_recv after the protocol layer was
 * split (was hal_usart_send_recv). hal_usart stays a stub (it touches hardware);
 * at_frame_* come from the real at_frame.c linked into sgc_mock (see CMake). */
void hal_usart_send(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}
bool hal_usart_rx_get(uint8_t *c)
{
    (void)c;
    return false;
}
bool hal_usart_rx_peek(uint8_t *c)
{
    (void)c;
    return false;
}
bool hal_usart_rx_peek_n(uint8_t *buf, uint16_t n)
{
    (void)buf;
    (void)n;
    return false;
}
void hal_usart_rx_clear(void) {}

/* at_frame.c feeds WWDGT in its wait loops (added during OTA stability work);
 * mock it as a no-op for host tests. */
void hal_wwdgt_feed(void) {}
