add_library(application_state EXCLUDE_FROM_ALL
    application_state.cc
)

target_include_directories(application_state
PUBLIC
    include
)

target_link_libraries(application_state
PUBLIC
    libmaelir_interface
    # This is an alias to the build target state
    generated_application_state
)
