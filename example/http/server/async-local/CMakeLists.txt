#
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(http-server-async-local
    Jamfile
    http_server_async_local.cpp)

source_group("" FILES
    Jamfile
    http_server_async_local.cpp)

target_link_libraries(http-server-async-local PRIVATE Boost::beast)

set_target_properties(http-server-async-local
    PROPERTIES FOLDER "example-http-server")
