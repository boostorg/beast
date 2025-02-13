#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(boost_beast_bench_utf8_checker
    Jamfile
    bench_utf8_checker.cpp)

source_group("" FILES
    Jamfile
    bench_utf8_checker.cpp)

target_link_libraries(boost_beast_bench_utf8_checker
    boost_beast_lib_test)

set_target_properties(boost_beast_bench_utf8_checker
    PROPERTIES FOLDER "tests-bench")
