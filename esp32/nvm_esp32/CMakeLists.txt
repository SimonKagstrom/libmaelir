add_library(nvm_esp32 EXCLUDE_FROM_ALL
    nvm_esp32.cc
)

target_include_directories(nvm_esp32
PUBLIC
    include
)

target_link_libraries(nvm_esp32
PUBLIC
    idf::freertos
    idf::nvs_flash
    libmaelir_interface
)
