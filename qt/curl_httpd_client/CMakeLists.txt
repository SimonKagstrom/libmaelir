find_package (CURL REQUIRED)

add_library(curl_httpd_client EXCLUDE_FROM_ALL
    curl_httpd_client.cc
)

target_link_libraries(curl_httpd_client
PUBLIC
    CURL::libcurl
    httpd_client_interface
)
