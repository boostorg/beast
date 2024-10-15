#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/extras/include/boost/beast extras)
GroupSources (test/beast/http "/")

add_executable (tests-beast-http
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    Jamfile
    any_completion_handler.cpp
    basic_dynamic_body_fwd.cpp
    basic_dynamic_body.cpp
    basic_file_body_fwd.cpp
    basic_file_body.cpp
    basic_parser.cpp
    buffer_body_fwd.cpp
    buffer_body.cpp
    chunk_encode.cpp
    deferred.cpp
    dynamic_body_fwd.cpp
    dynamic_body.cpp
    empty_body_fwd.cpp
    empty_body.cpp
    error.cpp
    field_compiles.cpp
    field.cpp
    fields_fwd.cpp
    fields.cpp
    file_body_fwd.cpp
    file_body.cpp
    message_fwd.cpp
    message_generator_fwd.cpp
    message_generator.cpp
    message.cpp
    parser_fwd.cpp
    parser.cpp
    read.cpp
    rfc7230.cpp
    serializer_fwd.cpp
    serializer.cpp
    span_body_fwd.cpp
    span_body.cpp
    status.cpp
    string_body_fwd.cpp
    string_body.cpp
    type_traits.cpp
    vector_body_fwd.cpp
    vector_body.cpp
    verb.cpp
    write.cpp
)

target_link_libraries(tests-beast-http
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-beast-http PROPERTY FOLDER "tests")

#
# Individual tests
#
#build_individual_tests(tests-beast-http LIBS lib-asio lib-asio-ssl lib-beast lib-test)
