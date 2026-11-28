include(FetchContent)

FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl.git
  GIT_TAG        20.49.0
  GIT_SHALLOW    TRUE
)

# No CMakeLists.txt, so only populated. Targets are defined below
FetchContent_Declare(
  pngdec
  GIT_REPOSITORY https://github.com/bitbank2/PNGdec.git
  GIT_TAG        1.1.7
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG        v9.6.0
  GIT_SHALLOW    TRUE
)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CONFIG_LV_BUILD_DEMOS OFF)
set(CONFIG_LV_BUILD_EXAMPLES OFF)

FetchContent_MakeAvailable(etl pngdec lvgl)
set(PNGDEC_SRC ${pngdec_SOURCE_DIR}/src)

if ("${IDF_TARGET}" STREQUAL "")
    # Build as shared for host, since this otherwise conflicts with libz
    set(LIB_TYPE SHARED)
    message("Building pngdec_zlib as shared library, to avoid host conflicts")
else()
    set(LIB_TYPE STATIC)
    message("Building pngdec_zlib as a static library")
endif()

add_library(pngdec_zlib ${LIB_TYPE} EXCLUDE_FROM_ALL
    ${PNGDEC_SRC}/adler32.c
    ${PNGDEC_SRC}/crc32.c
    ${PNGDEC_SRC}/infback.c
    ${PNGDEC_SRC}/inffast.c
    ${PNGDEC_SRC}/inflate.c
    ${PNGDEC_SRC}/inftrees.c
    ${PNGDEC_SRC}/zutil.c
)

target_compile_options(pngdec_zlib
PRIVATE
    -Wno-deprecated-non-prototype
)

add_library(pngdec ${LIB_TYPE} EXCLUDE_FROM_ALL
    ${PNGDEC_SRC}/PNGdec.cpp
)
target_link_libraries(pngdec
PRIVATE
    # Link privately, to avoid conflicts with system libz
    pngdec_zlib
)

target_compile_definitions(pngdec
PUBLIC
    -D__MACH__
)

target_include_directories(pngdec PUBLIC
    ${PNGDEC_SRC}
)
