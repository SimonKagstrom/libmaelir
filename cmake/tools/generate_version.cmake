# From kcov / https://www.marcusfolkesson.se/blog/git-version-in-cmake/
set(GIT_VERSION "<unknown>")
if (EXISTS "${SOURCE_DIRECTORY}/.git")
    find_program (GIT_EXECUTABLE NAMES git REQUIRED)

    execute_process (COMMAND "${GIT_EXECUTABLE}"
            -C "${SOURCE_DIRECTORY}"
            describe
            --always
            --abbrev=6
            --dirty
        WORKING_DIRECTORY ${SOURCE_DIRECTORY}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
message (STATUS "Version: ${GIT_VERSION}")

configure_file(${TEMPLATE} ${DST_FILE} @ONLY)
