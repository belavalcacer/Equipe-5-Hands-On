#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IR_MAX_SYMBOLS 256U
#define IR_RESOLUTION_HZ 1000000U
#define IR_DEFAULT_CARRIER_HZ 38000U
#define IR_MIN_CARRIER_HZ 20000U
#define IR_MAX_CARRIER_HZ 60000U
#define IR_MAX_DURATION 32767U

/* Application types, never a wire layout. 1 = MARK, 0 = SPACE. */
typedef struct {
    uint16_t duration;
    uint8_t level;
} ir_phase_t;

typedef struct {
    ir_phase_t phase0;
    ir_phase_t phase1;
} ir_symbol_t;

typedef struct {
    uint16_t symbol_count;
    uint16_t flags;
    uint32_t resolution_hz;
    uint32_t carrier_hz;
    ir_symbol_t symbols[IR_MAX_SYMBOLS];
} ir_code_t;

/* Zero duration is allowed only for the final SPACE (odd phase count). */
static inline bool ir_code_is_valid(const ir_code_t *code)
{
    if (!code || code->symbol_count == 0 || code->symbol_count > IR_MAX_SYMBOLS ||
        code->flags != 0 || code->resolution_hz != IR_RESOLUTION_HZ ||
        (code->carrier_hz != 0 &&
         (code->carrier_hz < IR_MIN_CARRIER_HZ || code->carrier_hz > IR_MAX_CARRIER_HZ))) {
        return false;
    }
    for (uint16_t i = 0; i < code->symbol_count; ++i) {
        const ir_symbol_t *s = &code->symbols[i];
        if (s->phase0.level != 1 || s->phase1.level != 0 ||
            s->phase0.duration == 0 || s->phase0.duration > IR_MAX_DURATION ||
            s->phase1.duration > IR_MAX_DURATION ||
            (s->phase1.duration == 0 && i + 1U != code->symbol_count)) {
            return false;
        }
    }
    return true;
}
