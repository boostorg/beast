#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(tests-beast-_experimental
    Jamfile
    _test_detail_stream_state.cpp
    error.cpp
    icy_stream.cpp
    stream.cpp)

source_group("" FILES
    Jamfile
    _test_detail_stream_state.cpp
    error.cpp
    icy_stream.cpp
    stream.cpp)

target_link_libraries(tests-beast-_experimental
    lib-asio-ssl
    lib-test)

set_target_properties(tests-beast-_experimental
    PROPERTIES FOLDER "tests")

add_test(NAME tests-beast-_experimental COMMAND tests-beast-_experimental)
add_dependencies(tests tests-beast-_experimental)
