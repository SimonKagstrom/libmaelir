add_subdirectory(../ libmaelir)
add_subdirectory(mock_filesystem)
add_subdirectory(mock_httpd_client)

add_library(os_unittest EXCLUDE_FROM_ALL
    os/base_thread_unittest.cc
    os/semaphore_unittest.cc
    # Maybe move to it's own library...
    mock_time.cc
)

target_link_libraries(os_unittest
    base_thread
    doctest::doctest
    trompeloeil::trompeloeil
)

target_include_directories(os_unittest
PUBLIC
    .
)
