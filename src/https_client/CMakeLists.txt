add_library(https_client_interface INTERFACE)

target_include_directories(https_client_interface
INTERFACE
    include
)

target_link_libraries(https_client_interface
INTERFACE
    libmaelir_interface
)


add_library(https_client INTERFACE)

target_link_libraries(https_client
INTERFACE
    https_client_interface
    https_client_implementation
)
