#pragma once
#include "esp_err.h"
#include "ir_types.h"

esp_err_t ir_tx_init(void);
/* Synchronous; ESP_ERR_TIMEOUT means another caller owns TX (BUSY). */
esp_err_t ir_tx_send(const ir_code_t *code);
