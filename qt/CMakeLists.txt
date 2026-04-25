add_subdirectory(.. libmaelir)

add_subdirectory(blitter_host)
add_subdirectory(curl_httpd_client)
add_subdirectory(display)
add_subdirectory(gpio_host)
add_subdirectory(pm_host)
add_subdirectory(nvm_host)
add_subdirectory(os)

# For the host
add_library(os_implementation ALIAS os_qt)
add_library(httpd_client_implementation ALIAS curl_httpd_client)
