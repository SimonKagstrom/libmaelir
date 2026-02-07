cmake_minimum_required (VERSION 3.21)

include (cmake/convert_font.cmake)

include(FetchContent)

FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl.git
  GIT_TAG        e9c2577d8e98ca9dd3a9c211cd2bb9395890978e # 20.45.0
)
FetchContent_MakeAvailable(etl)


# Note! The esp32/ or qt/ CMakeLists.txt should also be included for target-specifics
add_subdirectory(external)
add_subdirectory(src)
