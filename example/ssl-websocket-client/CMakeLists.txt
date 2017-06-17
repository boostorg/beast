# Part of Beast

GroupSources(include/beast beast)

GroupSources(example/ssl-websocket-client "/")

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable (ssl-websocket-client
    ${BEAST_INCLUDES}
    ssl_websocket_client.cpp
)

target_link_libraries(ssl-websocket-client Beast ${OPENSSL_LIBRARIES})
