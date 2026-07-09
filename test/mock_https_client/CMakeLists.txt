add_library(mock_https_client EXCLUDE_FROM_ALL
    mock_https_client.cc
)

target_link_libraries(mock_https_client
PUBLIC
    https_client_interface
    trompeloeil::trompeloeil
)

target_include_directories(mock_https_client
PUBLIC
    include
)
