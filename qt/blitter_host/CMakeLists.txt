add_library(blitter_host EXCLUDE_FROM_ALL
    blitter_host.cc
)

target_include_directories(blitter_host
PUBLIC
    include
)

target_link_libraries(blitter_host
PUBLIC
    libmaelir_interface
PRIVATE
    painter
)
