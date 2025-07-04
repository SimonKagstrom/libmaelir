add_library(jd9365_display_esp32 EXCLUDE_FROM_ALL
    jd9365_display_esp32.cc
)

target_include_directories(jd9365_display_esp32
PUBLIC
    include
)

target_link_libraries(jd9365_display_esp32
PUBLIC
    idf::freertos
    idf::esp_lcd
    idf::esp_timer
    idf::espressif__esp_lcd_jd9365
    libmaelir_interface
)
