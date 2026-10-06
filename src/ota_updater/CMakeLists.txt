add_library(ota_updater EXCLUDE_FROM_ALL
    ota_updater.cc
)

target_include_directories(ota_updater
PUBLIC
    include
)

target_link_libraries(ota_updater
PUBLIC
    application_state
    job_pool_thread
)
