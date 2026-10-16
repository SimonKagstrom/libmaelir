include(../cmake/generate_application_state.cmake)

# Only a base for the application's state, see generate_application_state()
declare_application_state(libmaelir_application_state
    FILES
        parameters_common.yml
        application_state_common.yml
)

target_link_libraries(libmaelir_application_state
INTERFACE
    libmaelir_interface
)

target_include_directories(libmaelir_application_state
INTERFACE
    include
)
