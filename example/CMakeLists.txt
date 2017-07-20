#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_subdirectory (echo-op)
add_subdirectory (http-client)
add_subdirectory (http-crawl)
add_subdirectory (http-server-fast)
add_subdirectory (http-server-small)
add_subdirectory (http-server-threaded)
add_subdirectory (server-framework)
add_subdirectory (websocket-client)
add_subdirectory (websocket-server-async)

if (OPENSSL_FOUND)
    add_subdirectory (http-client-ssl)
    add_subdirectory (websocket-client-ssl)
endif()
