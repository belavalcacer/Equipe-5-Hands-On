#include "ir_transport_uart.h"
#include "ir_rmt.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <limits.h>

static const char *TAG = "IR_UART";
static const uart_port_t port = CONFIG_IR_UART_PORT;
static QueueHandle_t events;
static SemaphoreHandle_t write_lock;
static size_t pending_bytes;

static bool reserved_pin(int gpio)
{
#if CONFIG_ESP_CONSOLE_UART_CUSTOM
    if (gpio == CONFIG_ESP_CONSOLE_UART_TX_GPIO || gpio == CONFIG_ESP_CONSOLE_UART_RX_GPIO) {
        return true;
    }
#endif
    /* WROVER flash/PSRAM, console and IR pins must remain dedicated. */
    return (gpio >= 6 && gpio <= 11) || gpio == 16 || gpio == 17 ||
           gpio == 1 || gpio == 3 || gpio == IR_RX_GPIO || gpio == IR_TX_GPIO;
}

esp_err_t ir_transport_init(void)
{
    if (write_lock) { return ESP_ERR_INVALID_STATE; }
    const int tx = CONFIG_IR_UART_TX_GPIO;
    const int rx = CONFIG_IR_UART_RX_GPIO;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(tx) || !GPIO_IS_VALID_GPIO(rx) || tx == rx ||
        reserved_pin(tx) || reserved_pin(rx)) {
        ESP_LOGE(TAG, "Invalid or reserved UART pins TX=%d RX=%d", tx, rx);
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_ESP_CONSOLE_UART
    if (port == CONFIG_ESP_CONSOLE_UART_NUM) {
        ESP_LOGE(TAG, "Binary UART cannot share the console");
        return ESP_ERR_INVALID_ARG;
    }
#endif
    const uart_config_t config = {
        .baud_rate = CONFIG_IR_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(port, &config);
    if (err != ESP_OK) { return err; }
    err = uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) { return err; }
    write_lock = xSemaphoreCreateMutex();
    if (!write_lock) { return ESP_ERR_NO_MEM; }
    err = uart_driver_install(port, 4096, 0, 32, &events, 0);
    if (err != ESP_OK) {
        vSemaphoreDelete(write_lock);
        write_lock = NULL;
        return err;
    }
    ESP_LOGI(TAG, "Binary protocol UART%d TX=%d RX=%d at %d baud, 8N1",
             port, tx, rx, CONFIG_IR_UART_BAUD_RATE);
    return ESP_OK;
}

int ir_transport_read(uint8_t *buffer, size_t max_len, TickType_t timeout)
{
    if (!events || !buffer || !max_len || max_len > INT_MAX) { return -1; }
    if (!pending_bytes) {
        uart_event_t event;
        if (xQueueReceive(events, &event, timeout) != pdTRUE) { return 0; }
        switch (event.type) {
        case UART_DATA:
            pending_bytes = event.size;
            break;
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
        case UART_BREAK:
        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART receive error %d; flushing input", event.type);
            uart_flush_input(port);
            xQueueReset(events);
            pending_bytes = 0;
            return -1;
        default:
            return 0;
        }
    }
    size_t amount = pending_bytes < max_len ? pending_bytes : max_len;
    if (!amount) { return 0; }
    int read = uart_read_bytes(port, buffer, amount, timeout);
    if (read <= 0) {
        uart_flush_input(port);
        xQueueReset(events);
        pending_bytes = 0;
        return -1;
    }
    pending_bytes -= (size_t)read;
    return read;
}

esp_err_t ir_transport_write(const uint8_t *buffer, size_t len)
{
    if (!write_lock) { return ESP_ERR_INVALID_STATE; }
    if (!buffer || !len || len > INT_MAX) { return ESP_ERR_INVALID_ARG; }
    xSemaphoreTake(write_lock, portMAX_DELAY);
    int written = uart_write_bytes(port, buffer, len);
    xSemaphoreGive(write_lock);
    return written == (int)len ? ESP_OK : ESP_FAIL;
}
