add_library(libmaelir_target_host EXCLUDE_FROM_ALL
    debug_assert.cc
)

target_include_directories(libmaelir_target_host
PUBLIC
    include
)
