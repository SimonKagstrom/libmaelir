add_compile_options(-Wdouble-promotion)

add_subdirectory(.. libmaelir)

# The display stuff is not possible to include on the io-boards
add_subdirectory(blitter_esp32)
add_subdirectory(can_esp32)
add_subdirectory(gpio_esp32)
add_subdirectory(gps_esp32)
add_subdirectory(httpd_ota_updater_esp32)
add_subdirectory(httpd_client_esp32)
add_subdirectory(nvm_esp32)
add_subdirectory(uart_esp32)
add_subdirectory(pm_esp32)
add_subdirectory(stepper_motor_esp32)
add_subdirectory(touch_esp32)
add_subdirectory(os)

add_library(httpd_client_implementation ALIAS httpd_client_esp32)

add_library(os_implementation ALIAS os_esp32)


target_compile_definitions(etl
INTERFACE
    ETL_TARGET_OS_FREERTOS
)

target_link_libraries(etl
INTERFACE
    idf::freertos
)

# esp-idf exposes freertos/FreeRTOS.h, but etl uses "FreeRTOS.h"
get_target_property(FREERTOS_INCLUDES idf::freertos INTERFACE_INCLUDE_DIRECTORIES)
foreach(inc_dir ${FREERTOS_INCLUDES})
    if(EXISTS "${inc_dir}/freertos")
        target_include_directories(etl INTERFACE "${inc_dir}/freertos")
    endif()
endforeach()