# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/websocket-server "/")

add_executable (websocket-server
    ${BEAST_INCLUDES}
    main.cpp
    websocket_async_echo_server.hpp
    websocket_sync_echo_server.hpp
)

target_link_libraries(websocket-server Beast)
