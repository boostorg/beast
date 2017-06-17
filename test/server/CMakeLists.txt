# Part of Beast

GroupSources(example/server-framework framework)
GroupSources(include/beast beast)

GroupSources(test/server "/")

if (OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(SSL_SOURCES
    https_ports.cpp
    ssl_stream.cpp
    )
else ()
  set(SSL_SOURCES "")
endif()

add_executable (server-test
    ${BEAST_INCLUDES}
    ${SERVER_INCLUDES}
    file_body.cpp
    file_service.cpp
    framework.cpp
    http_async_port.cpp
    http_base.cpp
    http_sync_port.cpp
    https_ports.cpp
    main.cpp
    rfc7231.cpp
    server.cpp
    service_list.cpp
    ssl_certificate
    write_msg.cpp
    ws_async_port.cpp
    ws_sync_port.cpp
    ws_upgrade_service.cpp
    wss_ports.cpp
    ${SSL_SOURCES}
)

target_link_libraries(server-test Beast)

if (OPENSSL_FOUND)
  target_link_libraries(server-test ${OPENSSL_LIBRARIES})
endif()
