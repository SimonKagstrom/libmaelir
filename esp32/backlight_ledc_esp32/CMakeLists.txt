add_library(backlight_ledc_esp32 EXCLUDE_FROM_ALL
    backlight_ledc_esp32.cc
)

target_include_directories(backlight_ledc_esp32
PUBLIC
    include
)

target_link_libraries(backlight_ledc_esp32
PUBLIC
    idf::esp_driver_gpio
    idf::esp_driver_ledc
    libmaelir_interface
)
