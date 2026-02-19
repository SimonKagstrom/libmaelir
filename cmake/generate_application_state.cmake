function(generate_application_state DEST_LIBRARY INPUT_FILE_LIST)
    set(OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DEST_LIBRARY}_generated/application_state/)

    set(out_cc ${OUTPUT_DIRECTORY}/generated_application_state.cc)
    set(out_hh ${OUTPUT_DIRECTORY}/generated_application_state.hh)

    message(STATUS "Generating application state")
    add_custom_command(
        OUTPUT ${out_cc} ${out_hh}
        COMMAND python3 ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generate_application_state.py ${OUTPUT_DIRECTORY} ${INPUT_FILE_LIST}
        WORKING_DIRECTORY ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
        DEPENDS
          ${INPUT_FILE_LIST}
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generated_application_state.hh.jinja2
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generated_application_state.cc.jinja2
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generate_application_state.py
        COMMENT "Generating application state from ${INPUT_FILE_LIST}"
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

    target_include_directories(${DEST_LIBRARY}
    PUBLIC
        ${OUTPUT_DIRECTORY}
    )
endfunction()
