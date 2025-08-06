add_library(gpio_host EXCLUDE_FROM_ALL
    gpio_host.cc
)

target_include_directories(gpio_host
PUBLIC
    include
)

target_link_libraries(gpio_host
PUBLIC
    libmaelir_interface
)
