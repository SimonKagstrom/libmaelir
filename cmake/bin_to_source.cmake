# Create a library (<BASE_NAME>_library) with the contents of DATA_FILE, as
#
#   extern const std::array<uint8_t, N> <BASE_NAME>_data;
#
# in <BASE_NAME>.hh.
function(library_from_data DATA_FILE BASE_NAME)
    _library_from_data(${DATA_FILE} ${BASE_NAME} "")
endfunction()

# As library_from_data, but with the PNG reduced to a 16 color palette (needs Pillow)
function(library_from_png_16_colors DATA_FILE BASE_NAME)
    _library_from_data(${DATA_FILE} ${BASE_NAME} --png-16-colors)
endfunction()

function(_library_from_data DATA_FILE BASE_NAME OPTIONS)
    set(DEST_LIBRARY ${BASE_NAME}_library)
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/${BASE_NAME}/)
    set(out_hh ${OUTPUT_DIR}/${BASE_NAME}.hh)
    set(out_cc ${OUTPUT_DIR}/${BASE_NAME}.cc)

    add_custom_command(
        OUTPUT ${out_cc} ${out_hh}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/bin_to_source.py
            ${OPTIONS}
            ${OUTPUT_DIR}
            ${DATA_FILE}
            ${BASE_NAME}

        DEPENDS ${DATA_FILE}
        DEPENDS ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/bin_to_source.py
    )

    add_custom_target(${DEST_LIBRARY}_target
        DEPENDS ${out_cc} ${out_hh}
    )

    # Library for the C++/header files
    add_library(${DEST_LIBRARY} STATIC EXCLUDE_FROM_ALL
        ${out_hh}
        ${out_cc}
    )

    # The address sanitizer is redundant for this (generated) data
    target_compile_options(${DEST_LIBRARY}
    PRIVATE
        -fno-sanitize=all
    )
    add_dependencies(${DEST_LIBRARY} ${DEST_LIBRARY}_target)

    target_include_directories(${DEST_LIBRARY}
    PUBLIC
        ${OUTPUT_DIR}
    )
endfunction()
