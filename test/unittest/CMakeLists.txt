cmake_minimum_required (VERSION 3.21)
project (maelir_unittest LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Enable sanitizers in debug builds
add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined -g)
include_directories(../../qt/lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

enable_testing()

add_subdirectory(.. libmaelir_unittest)

if(NOT TARGET display_properties)
    add_library(display_properties ALIAS display_800x800)
endif()

add_executable(unittest_libmaelir
    main.cc
    test_nmea_parser.cc
    test_opportunistic_scheduler.cc
    test_job_pool_thread.cc
    test_timer_manager.cc
)

target_link_libraries(unittest_libmaelir
    os_unittest
    opportunistic_semaphore
    nmea_parser
    job_pool_thread
    timer_manager
    doctest::doctest
    trompeloeil::trompeloeil
)
add_test(NAME unittest_libmaelir COMMAND unittest_libmaelir)
