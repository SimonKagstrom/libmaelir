cmake_minimum_required (VERSION 3.21)

include (cmake/convert_font.cmake)

include(FetchContent)

FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl.git
  GIT_TAG        912e22f238b707edc4d47d830f61f922cea79d70 # 20.47.1
)
FetchContent_MakeAvailable(etl)


# Note! The esp32/ or qt/ CMakeLists.txt should also be included for target-specifics
add_subdirectory(application_state)
add_subdirectory(external)
add_subdirectory(src)
