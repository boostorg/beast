#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(bench-buffers
    Jamfile
    bench_buffers.cpp)

source_group("" FILES
    Jamfile
    bench_buffers.cpp)

target_link_libraries(bench-buffers
    lib-test)

set_target_properties(bench-buffers
    PROPERTIES FOLDER "tests-bench")
