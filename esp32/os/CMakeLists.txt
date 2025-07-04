add_library(os_esp32 EXCLUDE_FROM_ALL
    base_thread_freertos.cc
    semaphore_freertos.cc
)

target_link_libraries(os_esp32
PUBLIC
    idf::freertos
    base_thread
    timer_manager
)
