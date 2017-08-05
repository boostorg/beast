#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(example/common common)
GroupSources(test/extras/include/boost/beast extras)
GroupSources(include/boost/beast beast)
GroupSources(test/example/common "/")

add_executable (tests-example-common
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    Jamfile
    detect_ssl.cpp
    root_certificates.cpp
    server_certificate.cpp
    session_alloc.cpp
    ssl_stream.cpp
    write_msg.cpp
    main.cpp
)
