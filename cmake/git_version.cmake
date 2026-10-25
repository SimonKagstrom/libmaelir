# Create an interface library (NAME) with version.hh, which has
#
#   constexpr auto kSoftwareVersion = "<git describe of SOURCE_DIRECTORY>";
#
# Regenerated on each build, but only rewritten (i.e., recompiled) when the version changes
function(git_version_header NAME SOURCE_DIRECTORY)
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/${NAME})

    add_custom_target(${NAME}_generate
        ${CMAKE_COMMAND} -D SOURCE_DIRECTORY=${SOURCE_DIRECTORY}
                         -D TEMPLATE=${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/version.hh.in
                         -D DST_FILE=${OUTPUT_DIR}/version.hh
                         -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/generate_version.cmake
        BYPRODUCTS ${OUTPUT_DIR}/version.hh
    )

    add_library(${NAME} INTERFACE)

    target_include_directories(${NAME}
    INTERFACE
        ${OUTPUT_DIR}
    )

    add_dependencies(${NAME}
        ${NAME}_generate
    )
endfunction()
