add_library(httpd_client_interface INTERFACE)

target_include_directories(httpd_client_interface
INTERFACE
    include
)

target_link_libraries(httpd_client_interface
INTERFACE
    libmaelir_interface
)


add_library(httpd_client INTERFACE)

target_link_libraries(httpd_client
INTERFACE
    httpd_client_interface
    httpd_client_implementation
)
