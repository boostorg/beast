#
# Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable (boost_beast_tests_ssl
    Jamfile
    ssl_stream.cpp)

source_group("" FILES
    Jamfile
    ssl_stream.cpp)

target_link_libraries(boost_beast_tests_ssl
    boost_beast_lib_asio_ssl
    boost_beast_lib_test)

set_target_properties(boost_beast_tests_ssl
    PROPERTIES FOLDER "tests")

add_test(NAME boost_beast_tests_ssl COMMAND boost_beast_tests_ssl)
add_dependencies(tests boost_beast_tests_ssl)
