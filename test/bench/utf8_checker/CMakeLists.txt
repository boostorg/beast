#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(bench-utf8-checker
    Jamfile
    bench_utf8_checker.cpp)

source_group("" FILES
    Jamfile
    bench_utf8_checker.cpp)

target_link_libraries(bench-utf8-checker
    lib-test)

set_target_properties(bench-utf8-checker
    PROPERTIES FOLDER "tests-bench")
