add_library(opportunistic_semaphore EXCLUDE_FROM_ALL
    opportunistic_scheduler.cc
    opportunistic_semaphore.cc
)

target_include_directories(opportunistic_semaphore
PUBLIC
    include
)

target_link_libraries(opportunistic_semaphore
PUBLIC
    os
)
