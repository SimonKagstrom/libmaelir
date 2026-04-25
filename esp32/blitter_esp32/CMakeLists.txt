add_library(blitter_esp32 EXCLUDE_FROM_ALL
    blitter_esp32.cc
)

target_include_directories(blitter_esp32
PUBLIC
    include
)

target_link_libraries(blitter_esp32
PUBLIC
    idf::driver
    idf::esp_driver_ppa
    idf::esp_mm
    libmaelir_interface
)
