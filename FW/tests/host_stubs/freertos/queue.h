#pragma once
#include <stddef.h>
#include "FreeRTOS.h"
typedef struct mock_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, size_t item_size);
int xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout);
int xQueueSendFromISR(QueueHandle_t queue, const void *item, BaseType_t *woken);
int xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout);
int xQueueReset(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);
