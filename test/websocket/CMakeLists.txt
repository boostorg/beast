# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/websocket "/")

add_executable (websocket-tests
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    ../../extras/beast/unit_test/main.cpp
    websocket_async_echo_server.hpp
    websocket_sync_echo_server.hpp
    doc_snippets.cpp
    error.cpp
    option.cpp
    rfc6455.cpp
    stream.cpp
    teardown.cpp
    frame.cpp
    mask.cpp
    utf8_checker.cpp
)

target_link_libraries(websocket-tests Beast Boost::program_options Boost::filesystem Boost::coroutine Boost::thread Boost::context)

if (MINGW)
    set_target_properties(websocket-tests PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj -Og")
endif()
