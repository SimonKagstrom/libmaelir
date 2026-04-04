add_library(os_esp32 EXCLUDE_FROM_ALL
    os_implementation_freertos.cc
    semaphore_freertos.cc
)

target_include_directories(os_esp32
PUBLIC
    include
)

target_link_libraries(os_esp32
PUBLIC
    idf::freertos
    base_thread
    timer_manager
)
