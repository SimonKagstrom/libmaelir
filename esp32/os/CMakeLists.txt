if(IDF_TARGET_ARCH STREQUAL "xtensa")
# Avoid unsupported relocations for IRAM code on xtensa
set_source_files_properties(semaphore_freertos.cc
PROPERTIES
    COMPILE_OPTIONS "-mtext-section-literals;-fno-jump-tables;-fno-tree-switch-conversion"
)
endif()

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
