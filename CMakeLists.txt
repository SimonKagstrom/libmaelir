cmake_minimum_required (VERSION 3.21)

include (cmake/convert_font.cmake)
include (cmake/bin_to_source.cmake)
include (cmake/generate_state_machine.cmake)
include (cmake/git_version.cmake)

# Note! The esp32/ or qt/ CMakeLists.txt should also be included for target-specifics
add_subdirectory(application_state)
add_subdirectory(external)
add_subdirectory(messages)
add_subdirectory(src)
