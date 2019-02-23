#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/extras/include/boost/beast extras)
GroupSources (test/beast/_experimental "/")

add_executable (tests-beast-_experimental
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    error.cpp
    icy_stream.cpp
    stream.cpp
)

set_property(TARGET tests-beast-_experimental PROPERTY FOLDER "tests")
