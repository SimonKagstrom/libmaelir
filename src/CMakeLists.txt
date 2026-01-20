add_subdirectory(base_thread)
add_subdirectory(button_debouncer)
add_subdirectory(displays)
add_subdirectory(filesystem)
add_subdirectory(httpd_client)
add_subdirectory(nmea_parser)
add_subdirectory(painter)
add_subdirectory(rotary_encoder)
add_subdirectory(std_filesystem)
add_subdirectory(timer_manager)
add_subdirectory(ui_menu)


add_library(libmaelir_interface INTERFACE)

target_include_directories(libmaelir_interface INTERFACE
    include
)

target_link_libraries(libmaelir_interface INTERFACE
    etl::etl
    display_properties
)
