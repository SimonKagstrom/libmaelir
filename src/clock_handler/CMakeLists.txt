add_library(clock_handler EXCLUDE_FROM_ALL
    clock_handler.cc
)

target_include_directories(clock_handler
PUBLIC
    include
)

target_link_libraries(clock_handler
PUBLIC
    application_state
    job_pool_thread
    # An alias to the application's messages (which include the common ones)
    messages
    post_office
)
