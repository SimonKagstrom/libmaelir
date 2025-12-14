add_library(pm_host EXCLUDE_FROM_ALL
    pm_host.cc
)

target_include_directories(pm_host
PUBLIC
    include
)

target_link_libraries(pm_host
PUBLIC
    libmaelir_interface
)
