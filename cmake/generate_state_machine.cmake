# Generate a state machine base class from a Mermaid state diagram in a markdown file
#
#   generate_state_machine(DEST_LIBRARY INPUT_FILE [CLASS_NAME name])
#
# Creates the interface library DEST_LIBRARY, which provides <stem>_base.hh, e.g.,
# wifi_state_machine.md -> wifi_state_machine_base.hh with the class WifiStateMachineBase.
# CLASS_NAME overrides the class name (default from the file name).
function(generate_state_machine DEST_LIBRARY INPUT_FILE)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "CLASS_NAME" "")

    get_filename_component(input_path ${INPUT_FILE} ABSOLUTE)
    get_filename_component(stem ${INPUT_FILE} NAME_WE)

    set(OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DEST_LIBRARY}_generated/state_machine/)
    set(out_hh ${OUTPUT_DIRECTORY}/${stem}_base.hh)

    set(class_name_arg)
    if(ARG_CLASS_NAME)
        set(class_name_arg --class-name ${ARG_CLASS_NAME})
    endif()

    message(STATUS "Generating state machine ${DEST_LIBRARY}")
    add_custom_command(
        OUTPUT ${out_hh}
        COMMAND python3 ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/generate_state_machine.py
            ${input_path} ${out_hh} ${class_name_arg}
        DEPENDS
          ${input_path}
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/generated_state_machine_base.hh.jinja2
          ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/generate_state_machine.py
        COMMENT "Generating state machine from ${input_path}"
    )

    add_custom_target(${DEST_LIBRARY}_target
    DEPENDS
        ${out_hh}
    )

    # Header-only
    add_library(${DEST_LIBRARY} INTERFACE)
    add_dependencies(${DEST_LIBRARY} ${DEST_LIBRARY}_target)

    target_include_directories(${DEST_LIBRARY}
    INTERFACE
        ${OUTPUT_DIRECTORY}
    )

    target_link_libraries(${DEST_LIBRARY}
    INTERFACE
        timer_manager
    )
endfunction()
