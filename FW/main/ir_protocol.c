#include "ir_protocol.h"
#include "ir_rx.h"
#include "ir_tx.h"
#include "ir_transport_uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "IR_PROTOCOL";
static uint8_t input[IR_PROTOCOL_MAX_PACKET];
static size_t used;
static uint8_t output[IR_PROTOCOL_MAX_PACKET];
static ir_code_t working_code;
static bool initialized;

uint16_t ir_read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t ir_read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void ir_write_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

void ir_write_u32_le(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) { p[i] = (uint8_t)(value >> (8U * i)); }
}

static uint32_t crc_update(uint32_t crc, const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0);
        }
    }
    return crc;
}

uint32_t ir_protocol_crc32(const uint8_t *header, const uint8_t *payload, size_t len)
{
    return crc_update(crc_update(0xFFFFFFFFU, header, 12), payload, len) ^ 0xFFFFFFFFU;
}

ir_status_t ir_protocol_decode_code(const uint8_t *payload, size_t len, ir_code_t *code)
{
    if (!payload || !code || len < IR_PROTOCOL_PAYLOAD_PREFIX_SIZE ||
        len > IR_PROTOCOL_MAX_PAYLOAD) { return IR_STATUS_INVALID_LENGTH; }
    uint16_t count = ir_read_u16_le(payload);
    if (count == 0 || count > IR_MAX_SYMBOLS) { return IR_STATUS_INVALID_PAYLOAD; }
    if (len != IR_PROTOCOL_PAYLOAD_PREFIX_SIZE + (size_t)count * 4U) {
        return IR_STATUS_INVALID_LENGTH;
    }
    code->symbol_count = count;
    code->flags = ir_read_u16_le(payload + 2);
    code->resolution_hz = ir_read_u32_le(payload + 4);
    code->carrier_hz = ir_read_u32_le(payload + 8);
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t a = ir_read_u16_le(payload + 12 + i * 4U);
        uint16_t b = ir_read_u16_le(payload + 14 + i * 4U);
        code->symbols[i].phase0 = (ir_phase_t){.duration = a & 0x7FFFU, .level = a >> 15};
        code->symbols[i].phase1 = (ir_phase_t){.duration = b & 0x7FFFU, .level = b >> 15};
    }
    return ir_code_is_valid(code) ? IR_STATUS_OK : IR_STATUS_INVALID_PAYLOAD;
}

esp_err_t ir_protocol_encode_code(const ir_code_t *code, uint8_t *payload,
                                  size_t capacity, size_t *len)
{
    if (!payload || !len || !ir_code_is_valid(code)) { return ESP_ERR_INVALID_ARG; }
    size_t size = IR_PROTOCOL_PAYLOAD_PREFIX_SIZE + code->symbol_count * 4U;
    if (capacity < size) { return ESP_ERR_INVALID_SIZE; }
    ir_write_u16_le(payload, code->symbol_count);
    ir_write_u16_le(payload + 2, code->flags);
    ir_write_u32_le(payload + 4, code->resolution_hz);
    ir_write_u32_le(payload + 8, code->carrier_hz);
    for (uint16_t i = 0; i < code->symbol_count; ++i) {
        const ir_symbol_t *s = &code->symbols[i];
        ir_write_u16_le(payload + 12 + i * 4U, (s->phase0.level << 15) | s->phase0.duration);
        ir_write_u16_le(payload + 14 + i * 4U, (s->phase1.level << 15) | s->phase1.duration);
    }
    *len = size;
    return ESP_OK;
}

static void respond(const uint8_t *request, ir_status_t status, size_t payload_len)
{
    ir_write_u16_le(output, IR_PROTOCOL_MAGIC);
    output[2] = IR_PROTOCOL_VERSION;
    output[3] = request[3];
    output[4] = IR_PROTOCOL_RESPONSE;
    output[5] = status;
    ir_write_u16_le(output + 6, ir_read_u16_le(request + 6));
    ir_write_u32_le(output + 8, payload_len);
    ir_write_u32_le(output + 12, ir_protocol_crc32(output, output + 16, payload_len));
    esp_err_t err = ir_transport_write(output, IR_PROTOCOL_HEADER_SIZE + payload_len);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Response failed: %s", esp_err_to_name(err)); }
}

static void process_request(size_t len)
{
    ir_status_t status = IR_STATUS_OK;
    size_t response_len = 0;
    /* Ignore incoming responses to avoid response loops. */
    if (input[4] & IR_PROTOCOL_RESPONSE) { return; }
    if (input[2] != IR_PROTOCOL_VERSION || input[4] != 0 || input[5] != 0) {
        respond(input, IR_STATUS_INVALID_PAYLOAD, 0);
        return;
    }
    esp_err_t err;
    switch (input[3]) {
    case IR_COMMAND_READ:
        if (len != 0) { status = IR_STATUS_INVALID_LENGTH; break; }
        err = ir_rx_capture(&working_code, CONFIG_IR_RX_CAPTURE_TIMEOUT_MS);
        if (err == ESP_ERR_TIMEOUT) { status = IR_STATUS_NO_DATA; }
        else if (err == ESP_ERR_NOT_FINISHED) { status = IR_STATUS_BUSY; }
        else if (err != ESP_OK) { status = IR_STATUS_INTERNAL_ERROR; }
        else if (ir_protocol_encode_code(&working_code, output + 16,
                                         IR_PROTOCOL_MAX_PAYLOAD, &response_len) != ESP_OK) {
            status = IR_STATUS_INTERNAL_ERROR;
            response_len = 0;
        }
        break;
    case IR_COMMAND_WRITE:
        status = ir_protocol_decode_code(input + 16, len, &working_code);
        if (status != IR_STATUS_OK) { break; }
        err = ir_tx_send(&working_code);
        if (err == ESP_ERR_TIMEOUT) { status = IR_STATUS_BUSY; }
        else if (err != ESP_OK) { status = IR_STATUS_TX_ERROR; }
        break;
    case IR_COMMAND_PING:
        if (len != 0) { status = IR_STATUS_INVALID_LENGTH; }
        break;
    default:
        status = IR_STATUS_INVALID_COMMAND;
        break;
    }
    respond(input, status, response_len);
}

static void discard(size_t len)
{
    used -= len;
    memmove(input, input + len, used);
}

static void parse_buffer(void)
{
    while (used >= 2) {
        if (input[0] != 'I' || input[1] != 'R') { discard(1); continue; }
        if (used < IR_PROTOCOL_HEADER_SIZE) { return; }
        uint32_t len = ir_read_u32_le(input + 8);
        if (len > IR_PROTOCOL_MAX_PAYLOAD) {
            if (!(input[4] & IR_PROTOCOL_RESPONSE)) { respond(input, IR_STATUS_INVALID_LENGTH, 0); }
            discard(1);
            continue;
        }
        size_t packet_len = IR_PROTOCOL_HEADER_SIZE + len;
        if (used < packet_len) { return; }
        if (ir_read_u32_le(input + 12) != ir_protocol_crc32(input, input + 16, len)) {
            if (!(input[4] & IR_PROTOCOL_RESPONSE)) { respond(input, IR_STATUS_CRC_ERROR, 0); }
            discard(1);
            continue;
        }
        process_request(len);
        discard(packet_len);
    }
}

void ir_protocol_feed(const uint8_t *bytes, size_t len)
{
    if (!bytes) { return; }
    for (size_t i = 0; i < len; ++i) {
        /* parse_buffer always consumes a full maximum-size packet. */
        if (used == sizeof(input)) { discard(1); }
        input[used++] = bytes[i];
        parse_buffer();
    }
}

void ir_protocol_reset(void) { used = 0; }

void ir_protocol_expire(void)
{
    /* Recover complete packets hidden behind a damaged, plausible length. */
    while (used) {
        discard(1);
        parse_buffer();
    }
}

static void protocol_task(void *arg)
{
    (void)arg;
    uint8_t bytes[128];
    int64_t last_byte = esp_timer_get_time();
    for (;;) {
        int count = ir_transport_read(bytes, sizeof(bytes), pdMS_TO_TICKS(50));
        if (count < 0) {
            ir_protocol_reset();
        } else if (count > 0) {
            ir_protocol_feed(bytes, (size_t)count);
            /* READ/TX may have blocked; idle timing starts after request processing. */
            last_byte = esp_timer_get_time();
        } else if (used && esp_timer_get_time() - last_byte >= IR_PROTOCOL_IDLE_TIMEOUT_US) {
            ESP_LOGD(TAG, "Incomplete packet timed out");
            ir_protocol_expire();
        }
    }
}

esp_err_t ir_protocol_init(void)
{
    if (initialized) { return ESP_ERR_INVALID_STATE; }
    ir_protocol_reset();
    if (xTaskCreate(protocol_task, "ir_protocol", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    initialized = true;
    ESP_LOGI(TAG, "Protocol v1 ready (READ / WRITE / PING)");
    return ESP_OK;
}
