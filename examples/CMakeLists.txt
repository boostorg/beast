# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(examples "/")

add_executable (http-crawl
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    urls_large_data.hpp
    urls_large_data.cpp
    http_crawl.cpp
)

add_executable (http-server
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    file_body.hpp
    mime_type.hpp
    http_async_server.hpp
    http_sync_server.hpp
    http_server.cpp
)

add_executable (http-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    http_example.cpp
)

add_executable (websocket-example
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    websocket_example.cpp
)
