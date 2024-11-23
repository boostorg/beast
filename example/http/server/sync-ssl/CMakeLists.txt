#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(http-server-sync-ssl
    Jamfile
    http_server_sync_ssl.cpp)

source_group("" FILES
    Jamfile
    http_server_sync_ssl.cpp)

target_include_directories(http-server-sync-ssl
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(http-server-sync-ssl
    PRIVATE Boost::beast OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(http-server-sync-ssl
    PROPERTIES FOLDER "example-http-server")
