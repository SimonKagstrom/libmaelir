add_compile_options(-Wdouble-promotion)

add_subdirectory(.. libmaelir)

# The display stuff is not possible to include on the io-boards
add_subdirectory(gpio_esp32)
add_subdirectory(gps_esp32)
add_subdirectory(httpd_ota_updater_esp32)
add_subdirectory(nvm_esp32)
add_subdirectory(uart_esp32)
add_subdirectory(os)
