add_library(st7701_display_esp32 EXCLUDE_FROM_ALL
    st7701_display_esp32.cc
)

target_include_directories(st7701_display_esp32
PUBLIC
    include
)

target_link_libraries(st7701_display_esp32
PUBLIC
    idf::freertos
    idf::esp_lcd
    idf::esp_timer
    idf::espressif__esp_lcd_st7701
    libmaelir_interface
)
