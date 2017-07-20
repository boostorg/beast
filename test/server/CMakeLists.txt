#
# Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(example/server-framework framework)
GroupSources(example/common common)
GroupSources(extras/boost/beast extras)
GroupSources(include/boost/beast beast)

GroupSources(test/server "/")

add_executable (server-test
    ${BOOST_BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    ${SERVER_INCLUDES}
    Jamfile
    ../../extras/boost/beast/unit_test/main.cpp
    file_service.cpp
    framework.cpp
    http_async_port.cpp
    http_base.cpp
    http_sync_port.cpp
    https_ports.cpp
    multi_port.cpp
    server.cpp
    service_list.cpp
    ssl_certificate
    tests.cpp
    ws_async_port.cpp
    ws_sync_port.cpp
    ws_upgrade_service.cpp
    wss_ports.cpp
)

target_link_libraries(server-test
    Beast
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    )

if (OPENSSL_FOUND)
    target_link_libraries (server-test ${OPENSSL_LIBRARIES})
endif()
