#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_subdirectory(_experimental)
add_subdirectory(core)
add_subdirectory(http)
add_subdirectory(ssl)
add_subdirectory(websocket)
add_subdirectory(zlib)

add_executable(boost_beast_tests_self_contained
    Jamfile
    core.cpp
    http.cpp
    ssl.cpp
    version.cpp
    websocket.cpp
    zlib.cpp)

source_group("" FILES
    Jamfile
    core.cpp
    http.cpp
    ssl.cpp
    version.cpp
    websocket.cpp
    zlib.cpp)

target_link_libraries(boost_beast_tests_self_contained
    boost_beast_lib_asio_ssl
    boost_beast_lib_test)

set_target_properties(boost_beast_tests_self_contained
    PROPERTIES FOLDER "tests")

add_test(NAME boost_beast_tests_self_contained COMMAND boost_beast_tests_self_contained)
add_dependencies(tests boost_beast_tests_self_contained)
