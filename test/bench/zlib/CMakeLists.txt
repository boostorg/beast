#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(boost_beast_bench_zlib
    Jamfile
    deflate_stream.cpp
    inflate_stream.cpp)

source_group("" FILES
    Jamfile
    deflate_stream.cpp
    inflate_stream.cpp)

target_include_directories(boost_beast_bench_zlib
    PRIVATE ${PROJECT_SOURCE_DIR}/test/extern)

target_link_libraries(boost_beast_bench_zlib
    boost_beast_lib_test
    boost_beast_lib_zlib)

set_target_properties(boost_beast_bench_zlib
    PROPERTIES FOLDER "tests-bench")
