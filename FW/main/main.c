#include "esp_err.h"
#include "ir_protocol.h"
#include "ir_rmt.h"
#include "ir_rx.h"
#include "ir_transport_uart.h"
#include "ir_tx.h"

void app_main(void)
{
    ESP_ERROR_CHECK(ir_rmt_init());
    ESP_ERROR_CHECK(ir_rx_init());
    ESP_ERROR_CHECK(ir_tx_init());
    ESP_ERROR_CHECK(ir_transport_init());
    ESP_ERROR_CHECK(ir_protocol_init());
}
