add_library(std_filesystem EXCLUDE_FROM_ALL
    std_filesystem.cc
)

target_link_libraries(std_filesystem
PUBLIC
    filesystem_interface
)
