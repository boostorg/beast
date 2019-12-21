#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

include_directories(../../../../../example/contrib/socks-client)

GroupSources (include/boost/beast beast)
#GroupSources (example/contrib/socks-client/socks socks)
GroupSources (test/example/contrib/socks-client/socks "/")


add_executable (socks-test
    ${BOOST_BEAST_FILES}
    ${TEST_MAIN}
    detail_char_type_test.cpp
    handshake_v4_test.cpp
#    handshake_v5_test.cpp
    socks5_auth_username_password_test.cpp
    test.cpp
)

target_link_libraries(socks-test
    lib-asio
    lib-beast
    lib-test
    )

set_property(TARGET socks-test PROPERTY FOLDER "tests-socks")
