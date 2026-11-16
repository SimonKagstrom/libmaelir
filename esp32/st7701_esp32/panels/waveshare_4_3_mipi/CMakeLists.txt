add_library(st7701_panel_waveshare_4_3 EXCLUDE_FROM_ALL
    st7701_panel_waveshare_4_3.cc
)

target_include_directories(st7701_panel_waveshare_4_3
PUBLIC
    include
)

target_link_libraries(st7701_panel_waveshare_4_3
PUBLIC
    st7701_display_esp32
)
