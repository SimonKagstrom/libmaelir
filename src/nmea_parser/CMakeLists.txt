generate_state_machine(nmea_parser_state_machine nmea_parser_state_machine.md)

add_library(nmea_parser EXCLUDE_FROM_ALL
    nmea_parser.cc
)

target_link_libraries(nmea_parser
PUBLIC
    libmaelir_interface
    nmea_parser_state_machine
)

target_include_directories(nmea_parser
PUBLIC
    include
)
