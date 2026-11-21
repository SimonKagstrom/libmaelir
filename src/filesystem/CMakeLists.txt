add_library(filesystem_interface INTERFACE)

target_include_directories(filesystem_interface
INTERFACE
    include
)

target_link_libraries(filesystem_interface
INTERFACE
    libmaelir_interface
)


# The parts common for all implementations, which link it. An object library, so that the
# objects end up in the implementation archive: with a static library, GNU ld would need it
# before the implementation on the link line, since it calls into the implementation
add_library(filesystem_common OBJECT EXCLUDE_FROM_ALL
    filesystem.cc
)

target_link_libraries(filesystem_common
PUBLIC
    filesystem_interface
)


add_library(filesystem INTERFACE)

target_link_libraries(filesystem
INTERFACE
    filesystem_interface
    # Basically for unit testing
    filesystem_implementation
)
