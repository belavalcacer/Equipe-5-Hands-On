#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef void *rmt_channel_handle_t;
typedef struct {
    uint16_t duration0 : 15;
    uint16_t level0 : 1;
    uint16_t duration1 : 15;
    uint16_t level1 : 1;
} rmt_symbol_word_t;
typedef struct {
    uint32_t signal_range_min_ns;
    uint32_t signal_range_max_ns;
    struct { unsigned en_partial_rx : 1; } flags;
} rmt_receive_config_t;
typedef struct {
    rmt_symbol_word_t *received_symbols;
    size_t num_symbols;
    struct { unsigned is_last : 1; } flags;
} rmt_rx_done_event_data_t;
typedef struct {
    bool (*on_recv_done)(rmt_channel_handle_t, const rmt_rx_done_event_data_t *, void *);
} rmt_rx_event_callbacks_t;
esp_err_t rmt_enable(rmt_channel_handle_t channel);
esp_err_t rmt_disable(rmt_channel_handle_t channel);
esp_err_t rmt_receive(rmt_channel_handle_t channel, void *buffer, size_t size,
                       const rmt_receive_config_t *config);
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t channel,
                                          const rmt_rx_event_callbacks_t *callbacks, void *context);
