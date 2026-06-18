find_package (CURL REQUIRED)

add_library(curl_https_client EXCLUDE_FROM_ALL
    curl_https_client.cc
)

target_link_libraries(curl_https_client
PUBLIC
    CURL::libcurl
    https_client_interface
)
