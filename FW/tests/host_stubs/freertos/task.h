#pragma once
#include "FreeRTOS.h"
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack,
                void *arg, unsigned priority, void *handle);
int xTaskCreatePinnedToCore(void (*task)(void *), const char *name, unsigned stack,
                           void *arg, unsigned priority, void *handle, int core);
int xPortGetCoreID(void);
