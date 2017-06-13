# Part of Beast

GroupSources(example/server-framework framework)
GroupSources(include/beast beast)

GroupSources(test/server "/")

add_executable (server-test
    ${BEAST_INCLUDES}
    ${SERVER_INCLUDES}
    file_body.cpp
    file_service.cpp
    framework.cpp
    http_async_port.cpp
    http_base.cpp
    http_sync_port.cpp
    main.cpp
    rfc7231.cpp
    server.cpp
    service_list.cpp
    write_msg.cpp
    ws_async_port.cpp
    ws_sync_port.cpp
    ws_upgrade_service.cpp
)

target_link_libraries(server-test Beast)

