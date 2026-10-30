#include "ir_tx.h"
#include "ir_rmt.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "IR_TX";
static SemaphoreHandle_t tx_lock;
static rmt_encoder_handle_t copy_encoder;
static rmt_symbol_word_t tx_symbols[IR_MAX_SYMBOLS];
static bool faulted;

esp_err_t ir_tx_init(void)
{
    if (tx_lock || !ir_rmt_tx_channel()) { return ESP_ERR_INVALID_STATE; }
    tx_lock = xSemaphoreCreateMutex();
    if (!tx_lock) { return ESP_ERR_NO_MEM; }
    const rmt_copy_encoder_config_t config = {};
    esp_err_t err = rmt_new_copy_encoder(&config, &copy_encoder);
    if (err == ESP_OK) { err = rmt_enable(ir_rmt_tx_channel()); }
    if (err != ESP_OK) {
        if (copy_encoder) { rmt_del_encoder(copy_encoder); copy_encoder = NULL; }
        vSemaphoreDelete(tx_lock);
        tx_lock = NULL;
        return err;
    }
    ESP_LOGI(TAG, "RAW TX ready, default carrier 38000 Hz / 33%%");
    return ESP_OK;
}

esp_err_t ir_tx_send(const ir_code_t *code)
{
    if (!ir_code_is_valid(code)) { return ESP_ERR_INVALID_ARG; }
    if (!tx_lock) { return ESP_ERR_INVALID_STATE; }
    if (xSemaphoreTake(tx_lock, 0) != pdTRUE) { return ESP_ERR_TIMEOUT; }
    if (faulted) {
        xSemaphoreGive(tx_lock);
        return ESP_ERR_INVALID_STATE;
    }
    const rmt_carrier_config_t carrier = {
        .frequency_hz = code->carrier_hz ? code->carrier_hz : IR_DEFAULT_CARRIER_HZ,
        .duty_cycle = 0.33f,
        .flags = {.polarity_active_low = false, .always_on = false},
    };
    esp_err_t err = rmt_apply_carrier(ir_rmt_tx_channel(), &carrier);
    if (err == ESP_OK) {
        for (uint16_t i = 0; i < code->symbol_count; ++i) {
            tx_symbols[i] = (rmt_symbol_word_t){
                .level0 = code->symbols[i].phase0.level,
                .duration0 = code->symbols[i].phase0.duration,
                .level1 = code->symbols[i].phase1.level,
                .duration1 = code->symbols[i].phase1.duration,
            };
        }
        const rmt_transmit_config_t config = {.loop_count = 0, .flags.eot_level = 0};
        err = rmt_transmit(ir_rmt_tx_channel(), copy_encoder, tx_symbols,
                           code->symbol_count * sizeof(tx_symbols[0]), &config);
        if (err == ESP_OK) {
            /* Never release or rewrite the buffer while the driver owns it. */
            err = rmt_tx_wait_all_done(ir_rmt_tx_channel(), -1);
            /* On an unexpected driver failure, preserve the buffer until reboot. */
            if (err != ESP_OK) { faulted = true; }
        }
    }
    if (err != ESP_OK) { ESP_LOGE(TAG, "Transmit failed: %s", esp_err_to_name(err)); }
    xSemaphoreGive(tx_lock);
    return err;
}
