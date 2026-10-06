#pragma once

#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"

#define IR_RX_GPIO 18
#define IR_TX_GPIO 19
/* An extra hardware block distinguishes full valid frames from overflow. */
#define IR_RX_MEM_SYMBOLS 320U
#define IR_TX_MEM_SYMBOLS 64U

esp_err_t ir_rmt_init(void);
rmt_channel_handle_t ir_rmt_rx_channel(void);
rmt_channel_handle_t ir_rmt_tx_channel(void);
