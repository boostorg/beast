#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(advanced-server-flex-awaitable
    Jamfile
    advanced_server_flex_awaitable.cpp)

source_group("" FILES
    Jamfile
    advanced_server_flex_awaitable.cpp)

target_include_directories(advanced-server-flex-awaitable
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(advanced-server-flex-awaitable
    PRIVATE Boost::beast OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(advanced-server-flex-awaitable
    PROPERTIES FOLDER "example-advanced-server")
