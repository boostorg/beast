#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

include_directories (extras/include)

file (GLOB_RECURSE EXTRAS_INCLUDES
    ${PROJECT_SOURCE_DIR}/test/extras/include/boost/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/test/extras/include/boost/beast/*.ipp
)

set (TEST_MAIN ${PROJECT_SOURCE_DIR}/test/extras/include/boost/beast/unit_test/main.cpp)

if (OPENSSL_FOUND)
    link_libraries (${OPENSSL_LIBRARIES})
endif()

add_subdirectory (beast)
add_subdirectory (bench)
add_subdirectory (doc)
add_subdirectory (example)
