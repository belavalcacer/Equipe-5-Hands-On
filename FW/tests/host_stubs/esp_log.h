#pragma once
/* Keep format checking and argument references in host builds. */
#include <stdio.h>
#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) { printf(__VA_ARGS__); } } while (0)
#define ESP_LOGW(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
