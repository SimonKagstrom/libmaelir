add_library(uart_esp32 EXCLUDE_FROM_ALL
    uart_esp32.cc
)

target_include_directories(uart_esp32
PUBLIC
    include
)

target_link_libraries(uart_esp32
PUBLIC
    idf::esp_driver_uart
    libmaelir_interface
)
