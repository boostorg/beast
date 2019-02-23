#
# Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/beast/ssl "/")

add_executable (tests-beast-ssl
    ${BOOST_BEAST_FILES}
    ${TEST_MAIN}
    Jamfile
    ssl_stream.cpp
)

set_property(TARGET tests-beast-ssl PROPERTY FOLDER "tests")
