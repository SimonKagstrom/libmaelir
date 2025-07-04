add_library(display_qt EXCLUDE_FROM_ALL
    display_qt.cc
)

target_link_libraries(display_qt
PUBLIC
    Qt6::Widgets
    libmaelir_interface
)
