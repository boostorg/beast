# Part of Beast

GroupSources(include/beast beast)

GroupSources(example/ssl-http-client "/")

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable (ssl-http-client
    ${BEAST_INCLUDES}
    ssl_http_client.cpp
)

target_link_libraries(ssl-http-client Beast ${OPENSSL_LIBRARIES})
