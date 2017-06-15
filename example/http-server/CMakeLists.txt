# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/http-server "/")

add_executable (http-server
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    file_body.hpp
    mime_type.hpp
    http_async_server.hpp
    http_sync_server.hpp
    main.cpp
)

if (NOT WIN32)
    target_link_libraries(http-server ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(http-server ${Boost_LIBRARIES})
endif()
