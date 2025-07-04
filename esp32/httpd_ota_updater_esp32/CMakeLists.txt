add_library(httpd_ota_updater_esp32 EXCLUDE_FROM_ALL
    httpd_ota_updater_esp32.cc
)

target_include_directories(httpd_ota_updater_esp32
PUBLIC
    include
)

target_link_libraries(httpd_ota_updater_esp32
PUBLIC
    idf::app_update
    idf::driver
    idf::esp_http_server
    idf::esp_netif
    idf::esp_partition
    idf::esp_wifi
    idf::esp_eth
    idf::freertos
    idf::nvs_flash
    idf::esp-tls
    idf::wifi_provisioning
    libmaelir_interface
)
