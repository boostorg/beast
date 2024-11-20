#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable (tests-beast-websocket
    Jamfile
    _detail_decorator.cpp
    _detail_prng.cpp
    _detail_impl_base.cpp
    test.hpp
    _detail_prng.cpp
    any_completion_handler.cpp
    accept.cpp
    cancel.cpp
    close.cpp
    deferred.cpp
    error.cpp
    frame.cpp
    handshake.cpp
    option.cpp
    ping.cpp
    read1.cpp
    read2.cpp
    read3.cpp
    rfc6455.cpp
    ssl.cpp
    stream.cpp
    stream_base.cpp
    stream_explicit.cpp
    stream_fwd.cpp
    teardown.cpp
    timer.cpp
    utf8_checker.cpp
    write.cpp)

target_link_libraries(tests-beast-websocket
    lib-asio-ssl
    lib-test)

set_target_properties(tests-beast-websocket
    PROPERTIES FOLDER "tests")
