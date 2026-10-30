#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
