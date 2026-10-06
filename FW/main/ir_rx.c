#include "ir_rx.h"
#include "ir_rmt.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "IR_RX";
typedef enum { RX_COMPLETE, RX_CAPTURE } rx_event_type_t;
typedef struct {
    rx_event_type_t type;
    size_t count;
    bool is_last;
    uint32_t timeout_ms;
    int64_t completed_us;
} rx_event_t;

static QueueHandle_t event_queue;
static SemaphoreHandle_t capture_lock;
static SemaphoreHandle_t capture_done;
static esp_err_t capture_result;
static bool running;
static bool faulted;
static int64_t deadline_us;
static ir_code_t pending_frame;
static rmt_symbol_word_t raw_symbols[IR_RX_MEM_SYMBOLS];
static const rmt_receive_config_t receive_config = {
    .signal_range_min_ns = 1000,
    .signal_range_max_ns = CONFIG_IR_RX_IDLE_US * 1000U,
    .flags.en_partial_rx = false,
};

static bool IRAM_ATTR rx_done(rmt_channel_handle_t channel,
                             const rmt_rx_done_event_data_t *data, void *context)
{
    (void)channel;
    rx_event_t event = {
        .type = RX_COMPLETE,
        .count = data->num_symbols,
        .is_last = data->flags.is_last,
        .completed_us = esp_timer_get_time(),
    };
    BaseType_t woken = pdFALSE;
    /* Timestamp and notify only; validation stays in the owner task. */
    xQueueSendFromISR((QueueHandle_t)context, &event, &woken);
    return woken == pdTRUE;
}

static bool normalize_frame(size_t count)
{
    if (count == 0 || count >= IR_RX_MEM_SYMBOLS) { return false; }
    memset(&pending_frame, 0, sizeof(pending_frame));
    pending_frame.resolution_hz = IR_RESOLUTION_HZ;
    /* A demodulated receiver cannot measure the carrier frequency. */
    pending_frame.carrier_hz = 0;
    size_t phases = 0;
    bool ended = false;
    for (size_t i = 0; i < count; ++i) {
        uint16_t durations[2] = {raw_symbols[i].duration0, raw_symbols[i].duration1};
        uint8_t levels[2] = {!raw_symbols[i].level0, !raw_symbols[i].level1};
        for (size_t j = 0; j < 2; ++j) {
            if (durations[j] == 0) {
                ended = true;
                break;
            }
            /* Discard electrical idle before the first MARK. */
            if (phases == 0 && levels[j] == 0) { continue; }
            if (phases >= IR_MAX_SYMBOLS * 2U || levels[j] != (phases % 2U == 0)) {
                return false;
            }
            ir_symbol_t *symbol = &pending_frame.symbols[phases / 2U];
            ir_phase_t *phase = phases % 2U ? &symbol->phase1 : &symbol->phase0;
            phase->duration = durations[j];
            phase->level = levels[j];
            ++phases;
        }
        if (ended) { break; }
    }
    pending_frame.symbol_count = (phases + 1U) / 2U;
    return ir_code_is_valid(&pending_frame);
}

static esp_err_t arm_receive(void)
{
    return rmt_receive(ir_rmt_rx_channel(), raw_symbols, sizeof(raw_symbols), &receive_config);
}

static void finish_capture(esp_err_t result)
{
    if (running) {
        esp_err_t err = rmt_disable(ir_rmt_rx_channel());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "RX disable failed: %s", esp_err_to_name(err));
            faulted = true;
            result = err;
        }
        running = false;
    }
    /* No new request can arrive until the current caller releases capture_lock.
     * This task shares the ISR CPU, so no old callback is still executing. */
    xQueueReset(event_queue);
    capture_result = result;
    xSemaphoreGive(capture_done);
}

static void start_capture(uint32_t timeout_ms)
{
    if (faulted) {
        finish_capture(ESP_ERR_INVALID_STATE);
        return;
    }
    memset(&pending_frame, 0, sizeof(pending_frame));
    esp_err_t err = rmt_enable(ir_rmt_rx_channel());
    if (err == ESP_OK) {
        running = true;
        deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
        err = arm_receive();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Capture start failed: %s", esp_err_to_name(err));
        finish_capture(err);
    } else {
        ESP_LOGI(TAG, "Capture requested, timeout %lu ms", (unsigned long)timeout_ms);
    }
}

/* Only this task operates RX, on the same CPU as the RMT interrupt. */
static void rx_task(void *arg)
{
    (void)arg;
    rx_event_t event;
    for (;;) {
        TickType_t wait = portMAX_DELAY;
        if (running) {
            int64_t remaining_us = deadline_us - esp_timer_get_time();
            /* Poll queued completions even at the deadline: their ISR timestamp
             * decides eligibility, not when this task gets scheduled. */
            const int64_t tick_us = (int64_t)portTICK_PERIOD_MS * 1000;
            wait = remaining_us > 0 ? (TickType_t)((remaining_us + tick_us - 1) / tick_us) : 0;
        }
        if (xQueueReceive(event_queue, &event, wait) != pdTRUE) {
            if (running && esp_timer_get_time() >= deadline_us) {
                ESP_LOGI(TAG, "Capture timed out without a valid frame");
                finish_capture(ESP_ERR_TIMEOUT);
            }
            continue;
        }
        if (event.type == RX_CAPTURE) {
            start_capture(event.timeout_ms);
        } else if (running) {
            if (event.completed_us > deadline_us) {
                finish_capture(ESP_ERR_TIMEOUT);
            } else if (event.is_last && normalize_frame(event.count)) {
                ESP_LOGD(TAG, "Captured %u symbols", pending_frame.symbol_count);
                finish_capture(ESP_OK);
            } else if (esp_timer_get_time() >= deadline_us) {
                finish_capture(ESP_ERR_TIMEOUT);
            } else {
                ESP_LOGD(TAG, "Discarding invalid capture (%u symbols)", (unsigned)event.count);
                /* Noise and oversized frames do not extend the request deadline. */
                esp_err_t err = arm_receive();
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Rearm failed: %s", esp_err_to_name(err));
                    finish_capture(err);
                }
            }
        }
    }
}

esp_err_t ir_rx_init(void)
{
    if (capture_lock || !ir_rmt_rx_channel()) { return ESP_ERR_INVALID_STATE; }
    capture_lock = xSemaphoreCreateMutex();
    capture_done = xSemaphoreCreateBinary();
    event_queue = xQueueCreate(2, sizeof(rx_event_t));
    esp_err_t err = ESP_ERR_NO_MEM;
    if (!capture_lock || !capture_done || !event_queue) { goto fail; }
    const rmt_rx_event_callbacks_t callbacks = {.on_recv_done = rx_done};
    err = rmt_rx_register_event_callbacks(ir_rmt_rx_channel(), &callbacks, event_queue);
    if (err != ESP_OK) { goto fail; }
    /* app_main initializes RMT and RX on the same pinned CPU. RX stays disabled. */
    if (xTaskCreatePinnedToCore(rx_task, "ir_rx", 4096, NULL, 10, NULL,
                               xPortGetCoreID()) != pdPASS) {
        const rmt_rx_event_callbacks_t empty = {};
        rmt_rx_register_event_callbacks(ir_rmt_rx_channel(), &empty, NULL);
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    return ESP_OK;
fail:
    if (event_queue) { vQueueDelete(event_queue); event_queue = NULL; }
    if (capture_done) { vSemaphoreDelete(capture_done); capture_done = NULL; }
    if (capture_lock) { vSemaphoreDelete(capture_lock); capture_lock = NULL; }
    return err;
}

esp_err_t ir_rx_capture(ir_code_t *code, uint32_t timeout_ms)
{
    if (!code || timeout_ms == 0 || timeout_ms > 60000) { return ESP_ERR_INVALID_ARG; }
    if (!capture_lock) { return ESP_ERR_INVALID_STATE; }
    if (xSemaphoreTake(capture_lock, 0) != pdTRUE) { return ESP_ERR_NOT_FINISHED; }
    const rx_event_t event = {.type = RX_CAPTURE, .timeout_ms = timeout_ms};
    xQueueSend(event_queue, &event, portMAX_DELAY);
    /* The owner enforces the deadline and disables RX before acknowledging.
     * Waiting for its acknowledgement prevents timeout/start races. */
    xSemaphoreTake(capture_done, portMAX_DELAY);
    esp_err_t err = capture_result;
    if (err == ESP_OK) { *code = pending_frame; }
    xSemaphoreGive(capture_lock);
    return err;
}
