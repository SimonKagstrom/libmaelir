add_library(os INTERFACE)

target_include_directories(os
INTERFACE
    include
)

target_link_libraries(os
INTERFACE
    libmaelir_interface
    # This is an alias to the build target OS implementation
    os_implementation
)
