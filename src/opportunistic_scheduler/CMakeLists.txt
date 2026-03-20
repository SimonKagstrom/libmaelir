add_library(opportunistic_scheduler EXCLUDE_FROM_ALL
    opportunistic_scheduler.cc
)

target_include_directories(opportunistic_scheduler
PUBLIC
    include
)

target_link_libraries(opportunistic_scheduler
PUBLIC
    os
)
