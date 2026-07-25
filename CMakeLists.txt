cmake_minimum_required (VERSION 3.21)

include (cmake/convert_font.cmake)

# Note! The esp32/ or qt/ CMakeLists.txt should also be included for target-specifics
add_subdirectory(application_state)
add_subdirectory(external)
add_subdirectory(src)
