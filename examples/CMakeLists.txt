# Part of Beast

GroupSources(include/beast)
GroupSources(examples)

add_executable (http-crawl
    ${BEAST_INCLUDES}
    http_crawl.cpp
    urls_large_data.cpp
)

if (NOT WIN32)
    target_link_libraries(http-crawl ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable (http-server
    ${BEAST_INCLUDES}
    http_server.cpp
)

if (NOT WIN32)
    target_link_libraries(http-server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable (http-example
    ${BEAST_INCLUDES}
    http_example.cpp
)

if (NOT WIN32)
    target_link_libraries(http-example ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable (websocket-echo
    ${BEAST_INCLUDES}
    websocket_echo.cpp
)

if (NOT WIN32)
    target_link_libraries(websocket-echo ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable (websocket-example
    ${BEAST_INCLUDES}
    websocket_example.cpp
)

if (NOT WIN32)
    target_link_libraries(websocket-example ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()
