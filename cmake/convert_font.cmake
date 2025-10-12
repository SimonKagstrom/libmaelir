# Find the lv_font_conv executable
find_program(LV_FONT_CONV_EXECUTABLE NAMES lv_font_conv REQUIRED)

function(convert_font BASE_NAME TTF SIZE_PIXELS EXTRA_SYMBOLS)
    set(DEST_LIBRARY ${BASE_NAME}_library)
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/${BASE_NAME}/)
    set(out_c ${OUTPUT_DIR}/${BASE_NAME}.c)
    set(out_h ${OUTPUT_DIR}/${BASE_NAME}.h)

    add_custom_command(
        OUTPUT ${out_c} ${out_h}
        WORKING_DIRECTORY ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../external/lvgl/scripts/built_in_font/
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../external/lvgl/scripts/built_in_font/built_in_font_gen.py
            --bpp 4
            --range 0x20-0x7f
            --symbols ${EXTRA_SYMBOLS}
            --size ${SIZE_PIXELS}
            -o ${out_c}

        COMMAND ${CMAKE_COMMAND} -E echo '\#include \"lvgl.h\"' > ${out_h}
        COMMAND ${CMAKE_COMMAND} -E echo 'extern const lv_font_t ${BASE_NAME}\;' >> ${out_h}
        DEPENDS ${TTF}
    )


    # Library for the C/header files
    add_library(${DEST_LIBRARY} STATIC EXCLUDE_FROM_ALL
        ${out_c}
    )
    add_custom_target(${DEST_LIBRARY}_target
        DEPENDS ${out_c}
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
    target_link_libraries(${DEST_LIBRARY}
    PUBLIC
        lvgl
    )
    
endfunction()
