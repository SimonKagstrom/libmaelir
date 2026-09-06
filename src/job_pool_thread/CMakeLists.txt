add_library(job_pool_thread EXCLUDE_FROM_ALL
    job_pool_thread.cc
)

target_include_directories(job_pool_thread
PUBLIC
    include
)

target_link_libraries(job_pool_thread
PUBLIC
    base_thread
    libmaelir_interface
)
