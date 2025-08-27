add_library(httpd_client_esp32 EXCLUDE_FROM_ALL
    httpd_client_esp32.cc
)

target_link_libraries(httpd_client_esp32
PUBLIC
    idf::esp_http_client
    httpd_client_interface
)
