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
GroupSources(test/benchmarks "/")
GroupSources(test/http "/")

add_executable (benchmarks
    ${BOOST_BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    Jamfile
    ../../extras/boost/beast/unit_test/main.cpp
    ../http/message_fuzz.hpp
    nodejs_parser.hpp
    buffers.cpp
    nodejs_parser.cpp
    parser.cpp
    utf8_checker.cpp
)
