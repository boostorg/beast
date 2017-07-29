#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

if (OPENSSL_FOUND)
    link_libraries (${OPENSSL_LIBRARIES})
endif()

add_subdirectory (beast)
add_subdirectory (doc)
add_subdirectory (example)

alias fat-tests : ;
