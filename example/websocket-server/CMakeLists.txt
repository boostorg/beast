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

if (NOT WIN32)
    target_link_libraries(websocket-server ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(websocket-server ${Boost_LIBRARIES})
endif()
