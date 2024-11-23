#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(tests-beast-zlib
    Jamfile
    error.cpp
    deflate_stream.cpp
    inflate_stream.cpp
    zlib.cpp)

source_group("" FILES
    Jamfile
    error.cpp
    deflate_stream.cpp
    inflate_stream.cpp
    zlib.cpp)

target_include_directories(tests-beast-zlib
    PRIVATE ${PROJECT_SOURCE_DIR}/test/extern)

target_link_libraries(tests-beast-zlib
    lib-test
    lib-zlib)

set_target_properties(tests-beast-zlib
    PROPERTIES FOLDER "tests")

add_test(NAME tests-beast-zlib COMMAND tests-beast-zlib)
add_dependencies(tests tests-beast-zlib)
