#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/common common)
GroupSources(example/http-client-ssl "/")

add_executable (http-client-ssl
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    Jamfile
    http_client_ssl.cpp
)

target_link_libraries (http-client-ssl
    ${OPENSSL_LIBRARIES}
    )
