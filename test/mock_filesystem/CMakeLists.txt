add_library(mock_filesystem EXCLUDE_FROM_ALL
    mock_filesystem.cc
)

target_link_libraries(mock_filesystem
PUBLIC
    filesystem_interface
    trompeloeil::trompeloeil
)

target_include_directories(mock_filesystem
PUBLIC
    include
)
