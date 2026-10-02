add_library(post_office EXCLUDE_FROM_ALL
    post_office.cc
)

target_include_directories(post_office
PUBLIC
    include
)

target_link_libraries(post_office
PUBLIC
    libmaelir_interface
    base_messages
)