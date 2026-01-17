add_library(can_esp32 EXCLUDE_FROM_ALL
    can_esp32.cc
)

target_include_directories(can_esp32
PUBLIC
    include
)

target_link_libraries(can_esp32
PUBLIC
    idf::driver
    libmaelir_interface
)
