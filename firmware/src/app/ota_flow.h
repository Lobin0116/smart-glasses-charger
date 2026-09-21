#ifndef OTA_FLOW_H
#define OTA_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "state_machine.h"
typedef void (*ota_progress_cb_t)(uint8_t percent);

void ota_init(void);

bool ota_request(sm_ctx_t *ctx);

bool ota_prepare(uint32_t *fw_size);

bool ota_read_block(uint16_t index, uint16_t block_size, uint8_t *data, uint16_t *data_len, uint8_t *type);

int ota_run(sm_ctx_t *ctx, ota_progress_cb_t progress_cb);

#endif
