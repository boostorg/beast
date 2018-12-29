#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2018 Brett Robinson (octo dot banana93 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/advanced/server-multi-io-context "/")

add_executable (advanced-server-multi-io-context
    ${BOOST_BEAST_FILES}
    Jamfile
    advanced_server_multi_io_context.cpp
)

set_property(TARGET advanced-server-multi-io-context PROPERTY FOLDER "example-advanced-server-multi-io-context")
