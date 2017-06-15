# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/ssl "/")

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable (http-ssl-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    http_ssl_example.cpp
)

target_link_libraries(http-ssl-example Beast ${OPENSSL_LIBRARIES})

add_executable (websocket-ssl-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    websocket_ssl_example.cpp
)

target_link_libraries(websocket-ssl-example Beast ${OPENSSL_LIBRARIES})
