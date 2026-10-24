add_library(unittest_messages INTERFACE)

target_include_directories(unittest_messages
INTERFACE
    include
)

target_link_libraries(unittest_messages
INTERFACE
    base_messages
)

# AllMessages for the unittests: the common messages and the unittest ones. In its own
# directory, since radbuzz provides its own messages.hh when building these tests
add_library(unittest_all_messages INTERFACE)

target_include_directories(unittest_all_messages
INTERFACE
    all_messages
)

target_link_libraries(unittest_all_messages
INTERFACE
    unittest_messages
)
