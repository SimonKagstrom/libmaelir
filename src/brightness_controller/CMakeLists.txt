add_library(brightness_controller EXCLUDE_FROM_ALL
    brightness_controller.cc
    sun_elevation.cc
)

target_include_directories(brightness_controller
PUBLIC
    include
)

target_link_libraries(brightness_controller
PUBLIC
    application_state
    job_pool_thread
)
