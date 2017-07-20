#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(example/common common)
GroupSources(extras/boost/beast extras)
GroupSources(include/boost/beast beast)
GroupSources(test/common "/")

add_executable (common-test
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    Jamfile
    detect_ssl.cpp
    mime_types.cpp
    rfc7231.cpp
    session_alloc.cpp
    ssl_stream.cpp
    write_msg.cpp
    main.cpp
)

target_link_libraries(common-test
    Beast
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    )

if (OPENSSL_FOUND)
    target_link_libraries (common-test ${OPENSSL_LIBRARIES})
endif()
