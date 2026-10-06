#pragma once
#include "esp_err.h"
#include "ir_types.h"

/* Initialize on the same pinned startup task as ir_rmt_init (app_main).
 * Creates the owner task without enabling reception. */
esp_err_t ir_rx_init(void);
/* Capture one new complete frame in task context (timeout_ms: 1..60000).
 * RX is disabled before returning; no previous frame is reused.
 * ESP_ERR_TIMEOUT: no valid frame completed before the deadline.
 * ESP_ERR_NOT_FINISHED: another capture caller already owns RX.
 * On failure, the caller's code buffer is left unchanged. */
esp_err_t ir_rx_capture(ir_code_t *code, uint32_t timeout_ms);
