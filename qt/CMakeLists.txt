add_subdirectory(.. libmaelir)

add_subdirectory(blitter_host)
add_subdirectory(curl_https_client)
add_subdirectory(display)
add_subdirectory(gpio_host)
add_subdirectory(ota_updater_host)
add_subdirectory(pm_host)
add_subdirectory(nvm_host)
add_subdirectory(os)
add_subdirectory(libmaelir_target_host)

# For the host
add_library(os_implementation ALIAS os_qt)
add_library(libmaelir_target ALIAS libmaelir_target_host)
add_library(https_client_implementation ALIAS curl_https_client)
