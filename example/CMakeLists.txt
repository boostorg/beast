# Part of Beast

add_subdirectory (echo-op)
add_subdirectory (http-client)
add_subdirectory (http-crawl)
add_subdirectory (http-server)
add_subdirectory (server-framework)
add_subdirectory (websocket-client)

if (OPENSSL_FOUND)
    add_subdirectory (http-client-ssl)
    add_subdirectory (websocket-client-ssl)
endif()
