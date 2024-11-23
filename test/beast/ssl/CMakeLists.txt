#
# Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable (tests-beast-ssl
    Jamfile
    ssl_stream.cpp)

source_group("" FILES
    Jamfile
    ssl_stream.cpp)

target_link_libraries(tests-beast-ssl
    lib-asio-ssl
    lib-test)

set_target_properties(tests-beast-ssl
    PROPERTIES FOLDER "tests")

add_test(NAME tests-beast-ssl COMMAND tests-beast-ssl)
add_dependencies(tests tests-beast-ssl)
