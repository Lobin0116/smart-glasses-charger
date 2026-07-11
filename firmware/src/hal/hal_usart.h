#ifndef HAL_USART_H
#define HAL_USART_H

#include <stdint.h>

/* Initialize USART0 for the single-wire POGO link: 921600 baud, 8N1, with DMA
 * on USART0_TX (channel 2) and USART0_RX (channel 1). The transceiver is left
 * in receive mode. The PA9/PA10 alternate-function pins are owned by hal_gpio. */
void hal_usart_init(void);

/* Transmit len bytes: drive T/R_SWITCH high, push the buffer through DMA, wait
 * for the final stop bit to leave the shift register, then release the line
 * back to receive mode. Returns the number of bytes shifted out. */
uint16_t hal_usart_send(const uint8_t *data, uint16_t len);

/* Capture up to maxlen bytes within timeout_ms. A response is framed by the
 * idle gap that follows its last byte; the call also returns early if the
 * buffer fills. The link must already be in receive mode. Returns the number of
 * bytes captured (0 if nothing arrived before the timeout). */
uint16_t hal_usart_recv(uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms);

/* Convenience request-response: transmit tx, then collect the reply into rx.
 * Returns the reply length, or 0 if no reply arrived within timeout_ms. */
uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max,
                             uint32_t timeout_ms);

#endif /* HAL_USART_H */
