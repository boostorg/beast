#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(http-server-sync
    Jamfile
    http_server_sync.cpp)

source_group("" FILES
    Jamfile
    http_server_sync.cpp)

target_link_libraries(http-server-sync PRIVATE Boost::beast)

set_target_properties(http-server-sync
    PROPERTIES FOLDER "example-http-server")
