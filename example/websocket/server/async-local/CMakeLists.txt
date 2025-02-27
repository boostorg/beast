#
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(websocket-server-async-local
    Jamfile
    websocket_server_async_local.cpp)

source_group("" FILES
    Jamfile
    websocket_server_async_local.cpp)

target_link_libraries(websocket-server-async-local PRIVATE Boost::beast)

set_target_properties(websocket-server-async-local
    PROPERTIES FOLDER "example-websocket-server")
