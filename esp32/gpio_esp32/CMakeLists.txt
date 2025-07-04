add_library(gpio_esp32 EXCLUDE_FROM_ALL
    gpio_esp32.cc
)

target_include_directories(gpio_esp32
PUBLIC
    include
)

target_link_libraries(gpio_esp32
PUBLIC
    idf::driver
    libmaelir_interface
)
