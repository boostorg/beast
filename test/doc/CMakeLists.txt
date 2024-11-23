#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS Jamfile *.cpp *.ipp *.hpp)

add_executable(tests-doc ${PFILES})

source_group("" FILES ${PFILES})

target_include_directories(tests-doc
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(tests-doc
    lib-asio-ssl
    lib-test)

set_target_properties(tests-doc
    PROPERTIES FOLDER "tests")

add_test(NAME tests-doc COMMAND tests-doc)
add_dependencies(tests tests-doc)
