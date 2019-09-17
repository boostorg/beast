#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copytight (c) 2018 jackarain (jack dot wgm at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/contrib/socks-client "/")

file (GLOB_RECURSE SOCKS_FILES
    socks/*.hpp
    socks/*.ipp
)

include_directories (.)

add_executable (socks-client-coro
    ${BOOST_BEAST_FILES}
    ${SOCKS_FILES}
    Jamfile
    socks_client_coro.cpp
)

target_link_libraries(socks-client-coro
    lib-asio
    lib-beast)

set_property(TARGET socks-client-coro PROPERTY FOLDER "contrib")
