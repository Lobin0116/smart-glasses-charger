#ifndef HAL_USART_H
#define HAL_USART_H

#include <stdbool.h>
#include <stdint.h>

void hal_usart_init(void);

bool hal_usart_rx_get(uint8_t *c);

bool hal_usart_rx_peek(uint8_t *c);

/* Peek up to n bytes from the RX ring buffer without advancing the read
 * pointer. Returns true if all n bytes are available, false otherwise (the
 * caller should wait for more bytes to arrive). Used by at_frame_recv's
 * opcode filter so it can inspect the opcode of an in-flight frame and leave
 * it in the buffer when the opcode is not what the caller expected. */
bool hal_usart_rx_peek_n(uint8_t *buf, uint16_t n);

uint16_t hal_usart_send(const uint8_t *data, uint16_t len);

uint16_t hal_usart_recv(uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms);

uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max, uint32_t timeout_ms);

#endif /* HAL_USART_H */
