#ifndef OTA_FLOW_H
#define OTA_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "state_machine.h"

/* OTA firmware transfer over the POGO link. The case drives the exchange: it
 * raises the OTA flag in the heartbeat, agrees on the image size, then pulls the
 * image block by block until the glasses mark the end, and finally clears the
 * flag to drop both sides back to normal operation. ota_run() runs the whole
 * sequence synchronously; the state machine parks in ST_OTA while it completes. */

/* Progress notification invoked from ota_run() with the completion percentage
 * (0-100). May be NULL when progress reporting is not wanted. */
typedef void (*ota_progress_cb_t)(uint8_t percent);

/* Reset the transfer state. Call once before starting a new OTA. */
void ota_init(void);

/* Raise the OTA flag in the heartbeat and wait for the glasses to acknowledge
 * (glass_sta bit7). Returns true once the glasses agreed. */
bool ota_request(sm_ctx_t *ctx);

/* Send the prepare command and read back the total firmware image size. On
 * success returns true and writes the size through fw_size. */
bool ota_prepare(uint32_t *fw_size);

/* Read one block at the given index. Up to block_size data bytes are written to
 * data; the byte count and the packet type (see at_packet_type_e) are returned
 * through data_len and type. data must hold at least block_size bytes. Returns
 * true on success. */
bool ota_read_block(uint16_t index, uint16_t block_size, uint8_t *data, uint16_t *data_len,
                    uint8_t *type);

/* Run the full OTA exchange to completion: request, prepare, read every block
 * until the glasses report the end, then clear the OTA flag and return to
 * normal. progress_cb is invoked with the completion percentage and may be NULL.
 * Returns 0 on success, a negative value on failure. */
int ota_run(sm_ctx_t *ctx, ota_progress_cb_t progress_cb);

#endif /* OTA_FLOW_H */
