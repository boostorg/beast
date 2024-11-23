#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(websocket-server-coro
    Jamfile
    websocket_server_coro.cpp)

source_group("" FILES
    Jamfile
    websocket_server_coro.cpp)

target_link_libraries(websocket-server-coro PRIVATE Boost::beast)

set_target_properties(websocket-server-coro
    PROPERTIES FOLDER "example-websocket-server")
