/* Deterministic owner-task scheduler: virtual time, queued ISR events, real RX code. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "ir_rx.h"
#include "ir_rmt.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct mock_queue {
    unsigned count;
    size_t size;
    unsigned char items[2][128];
};
struct mock_semaphore { bool available; bool binary; };
static struct mock_queue queue;
static struct mock_semaphore mutex, done;
static void (*owner_task)(void *);
static jmp_buf idle_return;
static int64_t now;
static bool enabled, armed, force_busy;
static unsigned enable_count, receive_count, disable_count;
static unsigned fail_receive_at;
static esp_err_t enable_error;
static rmt_rx_event_callbacks_t callbacks;
static void *callback_context;
static rmt_symbol_word_t *driver_buffer;
static struct {
    int64_t at;
    int64_t task_delay;
    size_t count;
    rmt_symbol_word_t symbols[IR_RX_MEM_SYMBOLS];
} plans[4];
static unsigned planned, next_plan;

int64_t esp_timer_get_time(void) { return now; }
int xPortGetCoreID(void) { return 0; }
rmt_channel_handle_t ir_rmt_rx_channel(void) { return &queue; }
QueueHandle_t xQueueCreate(unsigned length, size_t size)
{
    assert(length == 2 && size <= sizeof(queue.items[0]));
    queue.size = size;
    return &queue;
}
int xQueueSend(QueueHandle_t q, const void *item, TickType_t timeout)
{
    (void)timeout;
    assert(q->count < 2);
    memcpy(q->items[q->count++], item, q->size);
    return pdTRUE;
}
int xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken)
{
    *woken = pdFALSE;
    return xQueueSend(q, item, 0);
}
int xQueueReceive(QueueHandle_t q, void *item, TickType_t timeout)
{
    if (!q->count && timeout == portMAX_DELAY) {
        assert(done.available && !enabled && !armed);
        longjmp(idle_return, 1);
    }
    if (!q->count) {
        int64_t wake = now + (int64_t)timeout * 1000;
        if (armed && next_plan < planned && plans[next_plan].at <= wake) {
            unsigned index = next_plan++;
            assert(plans[index].at >= now);
            now = plans[index].at;
            memcpy(driver_buffer, plans[index].symbols, sizeof(plans[index].symbols));
            rmt_rx_done_event_data_t event = {
                .received_symbols = driver_buffer,
                .num_symbols = plans[index].count,
                .flags.is_last = true,
            };
            armed = false;
            callbacks.on_recv_done(ir_rmt_rx_channel(), &event, callback_context);
            now += plans[index].task_delay;
        } else {
            now = wake;
            return pdFALSE;
        }
    }
    memcpy(item, q->items[0], q->size);
    --q->count;
    memmove(q->items[0], q->items[1], q->count * q->size);
    return pdTRUE;
}
int xQueueReset(QueueHandle_t q) { q->count = 0; return pdTRUE; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    mutex = (struct mock_semaphore){.available = true}; return &mutex;
}
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    done = (struct mock_semaphore){.binary = true}; return &done;
}
int xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout)
{
    if (!semaphore->binary && force_busy) { assert(timeout == 0); return pdFALSE; }
    if (!semaphore->available && semaphore->binary) {
        assert(timeout == portMAX_DELAY);
        if (setjmp(idle_return) == 0) { owner_task(NULL); assert(false); }
    }
    assert(semaphore->available);
    semaphore->available = false;
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    assert(!semaphore->available);
    semaphore->available = true;
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t semaphore) { (void)semaphore; }
int xTaskCreatePinnedToCore(void (*task)(void *), const char *name, unsigned stack,
                           void *arg, unsigned priority, void *handle, int core)
{
    (void)name; (void)stack; (void)arg; (void)priority; (void)handle;
    assert(core == 0); owner_task = task; return pdPASS;
}
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t channel,
                                          const rmt_rx_event_callbacks_t *cb, void *context)
{
    (void)channel; callbacks = *cb; callback_context = context; return ESP_OK;
}
esp_err_t rmt_enable(rmt_channel_handle_t channel)
{
    (void)channel;
    assert(!enabled);
    if (enable_error != ESP_OK) { return enable_error; }
    enabled = true; ++enable_count; return ESP_OK;
}
esp_err_t rmt_disable(rmt_channel_handle_t channel)
{
    (void)channel;
    assert(enabled);
    enabled = false; armed = false; ++disable_count; return ESP_OK;
}
esp_err_t rmt_receive(rmt_channel_handle_t channel, void *buffer, size_t size,
                       const rmt_receive_config_t *config)
{
    (void)channel;
    assert(enabled && !armed);
    assert(size == IR_RX_MEM_SYMBOLS * sizeof(rmt_symbol_word_t));
    assert(!config->flags.en_partial_rx);
    ++receive_count;
    if (receive_count == fail_receive_at) { return ESP_FAIL; }
    driver_buffer = buffer; armed = true; return ESP_OK;
}
static void scenario(void)
{
    assert(!enabled && !armed && !queue.count);
    planned = next_plan = 0;
    memset(plans, 0, sizeof(plans));
    fail_receive_at = 0;
    enable_error = ESP_OK;
}
static void plan(int64_t delay_us, size_t count)
{
    assert(planned < 4);
    plans[planned].at = now + delay_us;
    plans[planned].count = count;
    plans[planned].symbols[0] = (rmt_symbol_word_t){
        .level0 = 0, .duration0 = 3320, .level1 = 1, .duration1 = 9936,
    };
    ++planned;
}
int main(void)
{
    ir_code_t code = {0};
    assert(ir_rx_capture(&code, 100) == ESP_ERR_INVALID_STATE);
    assert(ir_rx_init() == ESP_OK);
    assert(enable_count == 0 && receive_count == 0); /* Boot never enables RX. */
    assert(ir_rx_capture(NULL, 100) == ESP_ERR_INVALID_ARG);
    assert(ir_rx_capture(&code, 0) == ESP_ERR_INVALID_ARG);
    assert(ir_rx_capture(&code, 60001) == ESP_ERR_INVALID_ARG);
    force_busy = true;
    assert(ir_rx_capture(&code, 100) == ESP_ERR_NOT_FINISHED);
    force_busy = false;
    assert(enable_count == 0);

    scenario();
    int64_t start = now;
    assert(ir_rx_capture(&code, 100) == ESP_ERR_TIMEOUT);
    assert(now == start + 100000 && enable_count == disable_count);
    scenario();
    plan(20000, 1);
    assert(ir_rx_capture(&code, 100) == ESP_OK);
    assert(ir_code_is_valid(&code) && code.symbol_count == 1);
    assert(code.symbols[0].phase0.level == 1 && code.symbols[0].phase0.duration == 3320);
    assert(code.symbols[0].phase1.level == 0 && code.symbols[0].phase1.duration == 9936);
    assert(code.carrier_hz == 0 && enable_count == disable_count);
    /* Success is followed by timeout, never reuse of the previous frame. */
    scenario();
    ir_code_t previous = code;
    assert(ir_rx_capture(&code, 100) == ESP_ERR_TIMEOUT);
    assert(memcmp(&code, &previous, sizeof(code)) == 0);
    scenario();
    plan(10000, 0); /* Empty frame. */
    plan(20000, IR_RX_MEM_SYMBOLS); /* Overflow. */
    plan(30000, 1);
    assert(ir_rx_capture(&code, 100) == ESP_OK && next_plan == 3);
    scenario();
    start = now;
    plan(10000, 0);
    plan(90000, IR_RX_MEM_SYMBOLS);
    assert(ir_rx_capture(&code, 100) == ESP_ERR_TIMEOUT);
    assert(now == start + 100000); /* Invalid frames never extend the deadline. */
    scenario();
    plan(100000, 1); /* A complete frame exactly at the deadline is eligible. */
    plans[0].task_delay = 5000;
    assert(ir_rx_capture(&code, 100) == ESP_OK);
    scenario();
    plan(99500, 0);
    plan(100200, 1); /* Tick rounding wakes after the deadline: reject late data. */
    assert(ir_rx_capture(&code, 100) == ESP_ERR_TIMEOUT);
    scenario();
    plan(101000, 1); /* An incomplete/late frame cannot extend the request. */
    assert(ir_rx_capture(&code, 100) == ESP_ERR_TIMEOUT && next_plan == 0);
    scenario();
    fail_receive_at = receive_count + 1;
    assert(ir_rx_capture(&code, 100) == ESP_FAIL && !enabled);
    scenario();
    plan(10000, 0);
    fail_receive_at = receive_count + 2;
    assert(ir_rx_capture(&code, 100) == ESP_FAIL && !enabled);
    scenario();
    enable_error = ESP_FAIL;
    assert(ir_rx_capture(&code, 100) == ESP_FAIL && !enabled);
    scenario();
    plan(10000, 1);
    assert(ir_rx_capture(&code, 100) == ESP_OK);
    assert(enable_count == disable_count && !enabled && !armed);
    puts("RX tests passed (on-demand capture, deadlines, normalization, stale data, cleanup).");
    return 0;
}
