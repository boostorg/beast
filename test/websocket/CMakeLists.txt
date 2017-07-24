#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(extras/boost/beast extras)
GroupSources(include/boost/beast beast)

GroupSources(test/websocket "/")

add_executable (websocket-tests
    ${BOOST_BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    Jamfile
    ../../extras/boost/beast/unit_test/main.cpp
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

if (OPENSSL_FOUND)
    target_link_libraries(websocket-tests
        ${OPENSSL_LIBRARIES}
        )
endif()
