# Part of Beast

GroupSources(include/beast beast)

GroupSources(example/http-server "/")

add_executable (http-server
    ${BEAST_INCLUDES}
    fields_alloc.hpp
    http_server.cpp
)

target_link_libraries(http-server
    Beast
    ${Boost_FILESYSTEM_LIBRARY}
    )

