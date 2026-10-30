#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "ir_types.h"

#define IR_PROTOCOL_MAGIC 0x5249U
#define IR_PROTOCOL_VERSION 1U
#define IR_PROTOCOL_RESPONSE 0x01U
#define IR_PROTOCOL_HEADER_SIZE 16U
#define IR_PROTOCOL_PAYLOAD_PREFIX_SIZE 12U
#define IR_PROTOCOL_MAX_PAYLOAD (IR_PROTOCOL_PAYLOAD_PREFIX_SIZE + IR_MAX_SYMBOLS * 4U)
#define IR_PROTOCOL_MAX_PACKET (IR_PROTOCOL_HEADER_SIZE + IR_PROTOCOL_MAX_PAYLOAD)
#define IR_PROTOCOL_IDLE_TIMEOUT_US 250000

typedef enum {
    IR_COMMAND_READ = 0x01,
    IR_COMMAND_WRITE = 0x02,
    IR_COMMAND_PING = 0x03,
} ir_command_t;

/* Stable wire status values, also documented in docs/ir_protocol.md. */
typedef enum {
    IR_STATUS_OK = 0x00,
    IR_STATUS_INVALID_COMMAND = 0x01,
    IR_STATUS_INVALID_LENGTH = 0x02,
    IR_STATUS_INVALID_PAYLOAD = 0x03,
    IR_STATUS_CRC_ERROR = 0x04,
    IR_STATUS_NO_DATA = 0x05,
    IR_STATUS_BUSY = 0x06,
    IR_STATUS_TX_ERROR = 0x07,
    IR_STATUS_INTERNAL_ERROR = 0x08,
} ir_status_t;

esp_err_t ir_protocol_init(void);
/* Single-owner parser API. The protocol task owns it after init. */
void ir_protocol_feed(const uint8_t *bytes, size_t len);
void ir_protocol_reset(void);
void ir_protocol_expire(void);

uint16_t ir_read_u16_le(const uint8_t *p);
uint32_t ir_read_u32_le(const uint8_t *p);
void ir_write_u16_le(uint8_t *p, uint16_t value);
void ir_write_u32_le(uint8_t *p, uint32_t value);
/* CRC-32/ISO-HDLC over header[0:12] followed by payload (excludes CRC field). */
uint32_t ir_protocol_crc32(const uint8_t *header, const uint8_t *payload, size_t len);
ir_status_t ir_protocol_decode_code(const uint8_t *payload, size_t len, ir_code_t *code);
esp_err_t ir_protocol_encode_code(const ir_code_t *code, uint8_t *payload,
                                  size_t capacity, size_t *len);
