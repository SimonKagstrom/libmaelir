add_library(pm_esp32 EXCLUDE_FROM_ALL
    pm_esp32.cc
)

target_include_directories(pm_esp32
PUBLIC
    include
)

target_link_libraries(pm_esp32
PUBLIC
    idf::esp_pm
    libmaelir_interface
)
