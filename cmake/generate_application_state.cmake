# Application state from YAML files, which can be composed from a base, e.g.,
#
#   declare_application_state(common_state FILES parameters_common.yml)
#   generate_application_state(my_state BASE common_state FILES parameters.yml application_state.yml)
#
# The base files come before the own files, and relative paths are from the calling directory.
# The base is linked, so its usage requirements (include directories etc) come along. It can be
# defined before or after, since its files are only looked up at generate time.

# Collect the YAML files of BASE and FILES into OUT_VAR
function(_collect_application_state_files DEST_LIBRARY OUT_VAR)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "BASE" "FILES")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${DEST_LIBRARY}: Unknown arguments ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(files)
    if(ARG_BASE)
        # Evaluated at generate time, so the base can be defined after this. The base's
        # files can in turn refer to its base, hence TARGET_GENEX_EVAL
        set(files
            "$<TARGET_GENEX_EVAL:${ARG_BASE},$<TARGET_PROPERTY:${ARG_BASE},APPLICATION_STATE_FILES>>")
    endif()

    foreach(file IN LISTS ARG_FILES)
        get_filename_component(file ${file} ABSOLUTE)
        list(APPEND files ${file})
    endforeach()

    set(${OUT_VAR} ${files} PARENT_SCOPE)
    set(${OUT_VAR}_BASE ${ARG_BASE} PARENT_SCOPE)
endfunction()

# An application state which is only used as a base, i.e., without generated code
function(declare_application_state DEST_LIBRARY)
    _collect_application_state_files(${DEST_LIBRARY} files ${ARGN})

    add_library(${DEST_LIBRARY} INTERFACE)
    set_target_properties(${DEST_LIBRARY} PROPERTIES APPLICATION_STATE_FILES "${files}")
    if(files_BASE)
        target_link_libraries(${DEST_LIBRARY} INTERFACE ${files_BASE})
    endif()
endfunction()

function(generate_application_state DEST_LIBRARY)
    _collect_application_state_files(${DEST_LIBRARY} files ${ARGN})

    set(OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DEST_LIBRARY}_generated/application_state/)

    set(out_cc ${OUTPUT_DIRECTORY}/generated_application_state.cc)
    set(out_hh ${OUTPUT_DIRECTORY}/generated_application_state.hh)

    message(STATUS "Generating application state ${DEST_LIBRARY}")
    add_custom_command(
        OUTPUT ${out_cc} ${out_hh}
        COMMAND python3 ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/generate_application_state.py ${OUTPUT_DIRECTORY} ${files}
        WORKING_DIRECTORY ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
        DEPENDS
          ${files}
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/generated_application_state.hh.jinja2
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/generated_application_state.cc.jinja2
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/generate_application_state.py
        COMMENT "Generating application state ${DEST_LIBRARY}"
        COMMAND_EXPAND_LISTS
    )

    # Library for the C/header files
    add_library(${DEST_LIBRARY} STATIC EXCLUDE_FROM_ALL
        ${out_cc}
    )
    add_custom_target(${DEST_LIBRARY}_target
    DEPENDS
        ${out_cc}
        ${out_hh}
    )
    add_dependencies(${DEST_LIBRARY} ${DEST_LIBRARY}_target)
    set_target_properties(${DEST_LIBRARY} PROPERTIES APPLICATION_STATE_FILES "${files}")

    target_include_directories(${DEST_LIBRARY}
    PUBLIC
        ${OUTPUT_DIRECTORY}
    )
    if(files_BASE)
        target_link_libraries(${DEST_LIBRARY} PUBLIC ${files_BASE})
    endif()
endfunction()
