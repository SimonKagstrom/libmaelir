add_library(ota_updater_host EXCLUDE_FROM_ALL
    ota_updater_host.cc
)

target_include_directories(ota_updater_host
PUBLIC
    include
)

target_link_libraries(ota_updater_host
PUBLIC
    libmaelir_interface
)
