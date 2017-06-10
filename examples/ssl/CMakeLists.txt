# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(examples/ssl "/")

add_executable (http-ssl-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    http_ssl_example.cpp
)

target_link_libraries(http-ssl-example Beast OpenSSL::SSL)

add_executable (websocket-ssl-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    websocket_ssl_example.cpp
)

target_link_libraries(websocket-ssl-example Beast OpenSSL::SSL)
