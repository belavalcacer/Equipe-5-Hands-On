#include "ir_rmt.h"
#include "ir_types.h"
#include "esp_log.h"

static const char *TAG = "IR_RMT";
static rmt_channel_handle_t rx_channel;
static rmt_channel_handle_t tx_channel;

esp_err_t ir_rmt_init(void)
{
    if (rx_channel || tx_channel) { return ESP_ERR_INVALID_STATE; }
    const rmt_rx_channel_config_t rx = {
        .gpio_num = IR_RX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = IR_RX_MEM_SYMBOLS,
        .flags = {.invert_in = false, .with_dma = false},
    };
    const rmt_tx_channel_config_t tx = {
        .gpio_num = IR_TX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = IR_TX_MEM_SYMBOLS,
        .trans_queue_depth = 1,
        .flags = {.invert_out = false, .with_dma = false, .init_level = 0},
    };
    esp_err_t err = rmt_new_rx_channel(&rx, &rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RX creation failed: %s", esp_err_to_name(err));
        return err;
    }
    err = rmt_new_tx_channel(&tx, &tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX creation failed: %s", esp_err_to_name(err));
        rmt_del_channel(rx_channel);
        rx_channel = NULL;
        return err;
    }
    ESP_LOGI(TAG, "RX GPIO%d / TX GPIO%d, 1 MHz, no DMA", IR_RX_GPIO, IR_TX_GPIO);
    return ESP_OK;
}

rmt_channel_handle_t ir_rmt_rx_channel(void) { return rx_channel; }
rmt_channel_handle_t ir_rmt_tx_channel(void) { return tx_channel; }
