# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/websocket-client "/")

add_executable (websocket-client
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    websocket_client.cpp
)

if (NOT WIN32)
    target_link_libraries(websocket-client ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(websocket-client ${Boost_LIBRARIES})
endif()

