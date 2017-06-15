# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/http-client "/")

add_executable (http-client
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    http_client.cpp
)

if (NOT WIN32)
    target_link_libraries(http-client ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(http-client ${Boost_LIBRARIES})
endif()
