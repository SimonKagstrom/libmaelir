add_library(uart_gps_esp32 EXCLUDE_FROM_ALL
    uart_gps_esp32.cc
)

target_include_directories(uart_gps_esp32
PUBLIC
    include
)

target_link_libraries(uart_gps_esp32
PUBLIC
    idf::freertos
    uart_esp32
    idf::esp_timer
    libmaelir_interface
    nmea_parser
)


add_library(i2c_gps_esp32 EXCLUDE_FROM_ALL
    i2c_gps_esp32.cc
)

target_include_directories(i2c_gps_esp32
PUBLIC
    include
)

target_link_libraries(i2c_gps_esp32
PUBLIC
    idf::freertos
    idf::esp_driver_i2c
    nmea_parser
)
