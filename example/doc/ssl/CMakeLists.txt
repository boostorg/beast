#
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

# Client
add_executable(doc-ssl-client
    Jamfile
    client.cpp)

source_group("" FILES
    Jamfile
    client.cpp)

target_include_directories(doc-ssl-client
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(doc-ssl-client
    PRIVATE Boost::beast OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(doc-ssl-client
    PROPERTIES FOLDER "example-doc-ssl")

# Server
add_executable(doc-ssl-server
    Jamfile
    server.cpp)

source_group("" FILES
    Jamfile
    server.cpp)

target_include_directories(doc-ssl-server
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(doc-ssl-server
    PRIVATE Boost::beast OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(doc-ssl-server
    PROPERTIES FOLDER "example-doc-ssl")
