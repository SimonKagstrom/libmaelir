add_library(mock_httpd_client EXCLUDE_FROM_ALL
    mock_httpd_client.cc
)

target_link_libraries(mock_httpd_client
PUBLIC
    httpd_client_interface
    trompeloeil::trompeloeil
)

target_include_directories(mock_httpd_client
PUBLIC
    include
)
