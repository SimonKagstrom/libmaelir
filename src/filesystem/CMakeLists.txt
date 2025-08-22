add_library(filesystem_interface INTERFACE)

target_include_directories(filesystem_interface
INTERFACE
    include
)

target_link_libraries(filesystem_interface
INTERFACE
    libmaelir_interface
)


add_library(filesystem INTERFACE)

target_link_libraries(filesystem
INTERFACE
    filesystem_interface
    # Basically for unit testing
    filesystem_implementation
)
