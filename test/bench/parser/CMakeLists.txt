#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(bench-parser
    Jamfile
    nodejs_parser.hpp
    nodejs_parser.cpp
    bench_parser.cpp)

source_group("" FILES
    Jamfile
    nodejs_parser.hpp
    nodejs_parser.cpp
    bench_parser.cpp)

target_include_directories(bench-parser
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(bench-parser
    lib-test)

set_target_properties(bench-parser
    PROPERTIES FOLDER "tests-bench")
