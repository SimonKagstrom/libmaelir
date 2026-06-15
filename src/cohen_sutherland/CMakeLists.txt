add_library(cohen_sutherland INTERFACE)

target_include_directories(cohen_sutherland
INTERFACE
    include
)

target_link_libraries(cohen_sutherland
INTERFACE
    libmaelir_interface
)
