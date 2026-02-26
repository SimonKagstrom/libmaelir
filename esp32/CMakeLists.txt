add_compile_options(-Wdouble-promotion)

add_subdirectory(.. libmaelir)

# The display stuff is not possible to include on the io-boards
add_subdirectory(can_esp32)
add_subdirectory(gpio_esp32)
add_subdirectory(gps_esp32)
add_subdirectory(httpd_ota_updater_esp32)
add_subdirectory(httpd_client_esp32)
add_subdirectory(nvm_esp32)
add_subdirectory(uart_esp32)
add_subdirectory(pm_esp32)
add_subdirectory(stepper_motor_esp32)
add_subdirectory(os)

add_library(httpd_client_implementation ALIAS httpd_client_esp32)
