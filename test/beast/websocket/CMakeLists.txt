#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS Jamfile *.cpp *.hpp)

add_executable(tests-beast-websocket ${PFILES})

source_group("" FILES ${PFILES})

target_link_libraries(tests-beast-websocket
    lib-asio-ssl
    lib-test)

set_target_properties(tests-beast-websocket
    PROPERTIES FOLDER "tests")

add_test(NAME tests-beast-websocket COMMAND tests-beast-websocket)
add_dependencies(tests tests-beast-websocket)
