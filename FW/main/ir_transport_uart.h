#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t ir_transport_init(void);
/* Returns bytes read, zero on timeout, or -1 on loss/error (reset parser). */
int ir_transport_read(uint8_t *buffer, size_t max_len, TickType_t timeout);
esp_err_t ir_transport_write(const uint8_t *buffer, size_t len);
