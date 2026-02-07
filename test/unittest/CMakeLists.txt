cmake_minimum_required (VERSION 3.21)
project (maelir_unittest LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Enable sanitizers in debug builds
add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined -g)
include_directories(../../qt/lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

find_package(fmt REQUIRED)

enable_testing()

add_subdirectory(.. libmaelir_unittest)

add_executable(ut_libmaelir
    main.cc
    test_nmea_parser.cc
    test_timer_manager.cc
)

target_link_libraries(ut_libmaelir
    os_unittest
    nmea_parser
    timer_manager
    doctest::doctest
    trompeloeil::trompeloeil
)
add_test(NAME unittest COMMAND ut)
