#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(tests-example-common
    Jamfile
    root_certificates.cpp
    server_certificate.cpp)

source_group("" FILES
    Jamfile
    root_certificates.cpp
    server_certificate.cpp)

target_include_directories(tests-example-common
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(tests-example-common
    lib-asio-ssl
    lib-test)

set_target_properties(tests-example-common
    PROPERTIES FOLDER "tests")

add_test(NAME tests-example-common COMMAND tests-example-common)
add_dependencies(tests tests-example-common)
