#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(boost_beast_tests_example_common
    Jamfile
    root_certificates.cpp
    server_certificate.cpp)

source_group("" FILES
    Jamfile
    root_certificates.cpp
    server_certificate.cpp)

target_include_directories(boost_beast_tests_example_common
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(boost_beast_tests_example_common
    boost_beast_lib_asio_ssl
    boost_beast_lib_test)

set_target_properties(boost_beast_tests_example_common
    PROPERTIES FOLDER "tests")

add_test(NAME boost_beast_tests_example_common COMMAND boost_beast_tests_example_common)
add_dependencies(tests boost_beast_tests_example_common)
