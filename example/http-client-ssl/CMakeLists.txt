# Part of Beast

GroupSources(include/beast beast)

GroupSources(example/http-client-ssl "/")

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable (http-client-ssl
    ${BEAST_INCLUDES}
    http_client_ssl.cpp
)

target_link_libraries(http-client-ssl Beast ${OPENSSL_LIBRARIES})
