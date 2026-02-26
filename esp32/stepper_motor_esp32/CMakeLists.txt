add_library(stepper_motor_esp32 EXCLUDE_FROM_ALL
    stepper_motor_esp32.cc
)

target_include_directories(stepper_motor_esp32
PUBLIC
    include
)

target_link_libraries(stepper_motor_esp32
PUBLIC
    idf::esp_driver_rmt
    libmaelir_interface
)
