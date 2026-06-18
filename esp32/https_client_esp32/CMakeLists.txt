add_library(https_client_esp32 EXCLUDE_FROM_ALL
    https_client_esp32.cc
)

target_link_libraries(https_client_esp32
PUBLIC
    idf::esp_http_client
    https_client_interface
)
