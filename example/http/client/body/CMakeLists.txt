#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/client/coro "/")

add_executable (http-client-json
    ${BOOST_BEAST_FILES}
    Jamfile
    json_client.cpp
    )

target_link_libraries(http-client-json
    lib-asio
    lib-beast)

set_property(TARGET http-client-json PROPERTY FOLDER "example-json-client")
