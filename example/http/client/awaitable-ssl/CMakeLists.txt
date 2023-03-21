#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/client/awaitable "/")

add_executable (http-client-awaitable-ssl
    ${BOOST_BEAST_FILES}
    Jamfile
        http_client_awaitable_ssl.cpp
    )

target_link_libraries(http-client-awaitable-ssl
    OpenSSL::SSL OpenSSL::Crypto
    lib-asio
    lib-asio-ssl
    lib-beast)

set_property(TARGET http-client-awaitable-ssl PROPERTY FOLDER "example-http-client")
