#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/server-framework "/")
GroupSources(example/common "common")

file(GLOB_RECURSE SERVER_INCLUDES
    ${PROJECT_SOURCE_DIR}/example/server-framework/*.hpp
  )

add_executable (server-framework
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    ${SERVER_INCLUDES}
    Jamfile
    main.cpp
)

if (OPENSSL_FOUND)
    target_link_libraries(server-framework
        ${OPENSSL_LIBRARIES}
        )
endif()
