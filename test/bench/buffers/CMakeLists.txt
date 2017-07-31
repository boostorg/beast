#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(extras/boost/beast extras)
GroupSources(include/boost/beast beast)
GroupSources(test/bench/buffers "/")

add_executable (bench-buffers
    ${BOOST_BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    ${TEST_MAIN}
    Jamfile
    bench_buffers.cpp
)
