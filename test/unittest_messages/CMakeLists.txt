add_library(unittest_messages INTERFACE)

target_include_directories(unittest_messages
INTERFACE
    include
)

target_link_libraries(unittest_messages
INTERFACE
    base_messages
)
