include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        1da23a3e8119ec5cce4f9388e91b065e20bf06f5 # v2.4.12
)
FetchContent_Declare(
  trompeloeil
  GIT_REPOSITORY https://github.com/rollbear/trompeloeil.git
  GIT_TAG        eaeb89c1ce9d354b0ba7eb921fd5712cdbd78adf # v49
)
FetchContent_MakeAvailable(doctest trompeloeil)


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
