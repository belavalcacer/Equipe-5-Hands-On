idf_component_register(
    SRCS "main.c" "ir_rmt.c" "ir_rx.c" "ir_tx.c"
         "ir_protocol.c" "ir_transport_uart.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_rmt esp_driver_uart esp_driver_gpio freertos esp_timer
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
