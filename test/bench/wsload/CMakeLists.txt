#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/common common)
GroupSources(test/extras/include/boost/beast extras)
GroupSources(subtree/unit_test/include/boost/beast extras)
GroupSources(test/bench/wsload "/")

add_executable (bench-wsload
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    ${EXTRAS_INCLUDES}
    Jamfile
    wsload.cpp
    )
