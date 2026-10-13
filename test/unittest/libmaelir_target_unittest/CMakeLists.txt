add_library(libmaelir_target_unittest EXCLUDE_FROM_ALL
    debug_assert.cc
)

target_include_directories(libmaelir_target_unittest
PUBLIC
    include
)

target_link_libraries(libmaelir_target_unittest
PUBLIC
    doctest::doctest
    trompeloeil::trompeloeil
)
