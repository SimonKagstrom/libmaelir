include(../cmake/generate_application_state.cmake)

set(LIBMAELIR_APPLICATION_STATE
    "${CMAKE_CURRENT_LIST_DIR}/parameters_common.yml"
    "${CMAKE_CURRENT_LIST_DIR}/application_state_common.yml"
)
set(LIBMAELIR_APPLICATION_STATE ${LIBMAELIR_APPLICATION_STATE} PARENT_SCOPE)
list(JOIN LIBMAELIR_APPLICATION_STATE ";" LIBMAELIR_APPLICATION_STATE_STR)

generate_application_state(libmaelir_application_state "${LIBMAELIR_APPLICATION_STATE_STR}")


target_link_libraries(libmaelir_application_state
PUBLIC
    libmaelir_interface
)

target_include_directories(libmaelir_application_state
PUBLIC
    include
)
