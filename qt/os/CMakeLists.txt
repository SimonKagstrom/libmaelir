add_library(os_qt EXCLUDE_FROM_ALL
    base_thread_qt.cc
    os_implementation_qt.cc
    semaphore_qt.cc
)

target_include_directories(os_qt
PUBLIC
    include
)

target_link_libraries(os_qt
PUBLIC
    Qt6::Widgets
    base_thread
)
