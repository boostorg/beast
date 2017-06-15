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

target_link_libraries(http-server Beast ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} )
