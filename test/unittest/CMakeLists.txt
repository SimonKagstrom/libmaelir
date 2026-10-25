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
add_subdirectory(libmaelir_target_unittest)

add_library(libmaelir_target ALIAS libmaelir_target_unittest)

if(NOT TARGET display_properties)
    add_library(display_properties ALIAS display_800x800)
endif()

# Only the common state and messages. When built from an application's unittests, the
# application's (which include the common ones) are used instead
if(NOT TARGET generated_application_state)
    generate_application_state(libmaelir_unittest_application_state
        BASE
            libmaelir_application_state
    )
    add_library(generated_application_state ALIAS libmaelir_unittest_application_state)
endif()
if(NOT TARGET messages)
    add_library(messages ALIAS unittest_all_messages)
endif()

generate_state_machine(lamp_state_machine_base lamp_state_machine.md)

add_executable(unittest_libmaelir
    main.cc
    test_brightness_controller.cc
    test_clock_handler.cc
    test_nmea_parser.cc
    test_opportunistic_scheduler.cc
    test_painter.cc
    test_job_pool_thread.cc
    test_listener_cookie.cc
    test_state_machine.cc
    test_timer_manager.cc
    test_post_office.cc
)

target_link_libraries(unittest_libmaelir
    base_thread
    brightness_controller
    clock_handler
    doctest::doctest
    job_pool_thread
    lamp_state_machine_base
    nmea_parser
    opportunistic_semaphore
    os_unittest
    painter
    post_office
    timer_manager
    trompeloeil::trompeloeil
    unittest_messages
)
add_test(NAME unittest_libmaelir COMMAND unittest_libmaelir)
