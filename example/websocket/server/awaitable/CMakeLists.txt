#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/websocket/server/awaitable "/")

add_executable (websocket-server-awaitable
    ${BOOST_BEAST_FILES}
    Jamfile
        websocket_server_awaitable.cpp
)

target_link_libraries(websocket-server-awaitable
    lib-asio
    lib-beast)

set_property(TARGET websocket-server-awaitable PROPERTY FOLDER "example-websocket-server")
