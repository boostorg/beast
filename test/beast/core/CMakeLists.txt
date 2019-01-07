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
GroupSources (test/beast/core "/")

add_executable (tests-beast-core
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    _detail_base64.cpp
    _detail_buffer.cpp
    _detail_clamp.cpp
    _detail_read.cpp
    _detail_sha1.cpp
    _detail_tuple.cpp
    _detail_variant.cpp
    _detail_varint.cpp
    async_op_base.cpp
    buffer_test.hpp
    file_test.hpp
    basic_stream_socket.cpp
    bind_handler.cpp
    buffer_traits.cpp
    buffered_read_stream.cpp
    buffers_adapter.cpp
    buffers_adaptor.cpp
    buffers_cat.cpp
    buffers_prefix.cpp
    buffers_range.cpp
    buffers_suffix.cpp
    buffers_to_string.cpp
    error.cpp
    file.cpp
    file_posix.cpp
    file_stdio.cpp
    file_win32.cpp
    flat_buffer.cpp
    flat_static_buffer.cpp
    handler_ptr.cpp
    make_printable.cpp
    multi_buffer.cpp
    ostream.cpp
    read_size.cpp
    span.cpp
    static_buffer.cpp
    static_string.cpp
    stream_socket.cpp
    string.cpp
    string_param.cpp
    type_traits.cpp
)

set_property(TARGET tests-beast-core PROPERTY FOLDER "tests")
