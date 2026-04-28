add_library(touch_esp32 EXCLUDE_FROM_ALL
    touch_esp32.cc
)

target_include_directories(touch_esp32
PUBLIC
    include
)

target_link_libraries(touch_esp32
PUBLIC
    idf::espressif__esp_lcd_touch
    libmaelir_interface
)
