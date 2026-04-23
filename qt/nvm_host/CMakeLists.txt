add_library(nvm_host EXCLUDE_FROM_ALL
    nvm_host.cc
)

target_include_directories(nvm_host
PUBLIC
    include
)

target_link_libraries(nvm_host
PUBLIC
    libmaelir_interface
)
