add_library(ui_menu EXCLUDE_FROM_ALL
    menu_screen.cc
)

target_link_libraries(ui_menu
PUBLIC
    libmaelir_interface
    lvgl
    timer_manager
)

target_include_directories(ui_menu
PUBLIC
    include
)
