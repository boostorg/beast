//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_READ_IPP_HPP
#define BEAST_HTTP_IMPL_READ_IPP_HPP

#include <beast/http/concepts.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message_parser.hpp>
#include <beast/http/read.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

namespace detail {

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
inline
std::size_t
read_some_buffer(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
    error_code& ec)
{
    std::size_t bytes_used;
    if(dynabuf.size() == 0)
        goto do_read;
    for(;;)
    {
        bytes_used = parser.write(
            dynabuf.data(), ec);
        if(ec)
            return 0;
        if(bytes_used > 0)
            goto do_finish;
    do_read:
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        auto const size =
            read_size_helper(dynabuf, 65536);
        BOOST_ASSERT(size > 0);
        try
        {
            mb.emplace(dynabuf.prepare(size));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return 0;
        }
        auto const bytes_transferred =
            stream.read_some(*mb, ec);
        if(ec == boost::asio::error::eof)
        {
            BOOST_ASSERT(bytes_transferred == 0);
            bytes_used = 0;
            if(parser.got_some())
            {
                // caller sees EOF on next read
                ec = {};
                parser.write_eof(ec);
                if(ec)
                    return 0;
                BOOST_ASSERT(parser.is_complete());
                break;
            }
            ec = error::end_of_stream;
            break;
        }
        else if(ec)
        {
            return 0;
        }
        BOOST_ASSERT(bytes_transferred > 0);
        dynabuf.commit(bytes_transferred);
    }
do_finish:
    return bytes_used;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
inline
std::size_t
read_some_body(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
    error_code& ec)
{
    if(dynabuf.size() > 0)
        return parser.copy_body(dynabuf);
    boost::optional<typename
        Derived::mutable_buffers_type> mb;
    try
    {
        parser.prepare_body(mb, 65536);
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return 0;
    }
    auto const bytes_transferred =
        stream.read_some(*mb, ec);
    if(ec == boost::asio::error::eof)
    {
        BOOST_ASSERT(bytes_transferred == 0);
        // caller sees EOF on next read
        ec = {};
        parser.write_eof(ec);
        if(ec)
            return 0;
        BOOST_ASSERT(parser.is_complete());
    }
    else if(! ec)
    {
        parser.commit_body(bytes_transferred);
        return 0;
    }
    return 0;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
inline
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
    error_code& ec)
{
    switch(parser.state())
    {
    case parse_state::header:
    case parse_state::chunk_header:
        return detail::read_some_buffer(
            stream, dynabuf, parser, ec);

    default:
        return detail::read_some_body(
            stream, dynabuf, parser, ec);
    }
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
inline
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, false, Derived>& parser,
    error_code& ec)
{
    return detail::read_some_buffer(
        stream, dynabuf, parser, ec);
}

} // detail

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    error_code ec;
    auto const bytes_used =
        read_some(stream, dynabuf, parser, ec);
    if(ec)
        throw system_error{ec};
    return bytes_used;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    return detail::read_some(stream, dynabuf, parser, ec);
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
read(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    error_code ec;
    read(stream, dynabuf, parser, ec);
    if(ec)
        throw system_error{ec};
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
read(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    do
    {
        auto const bytes_used =
            read_some(stream, dynabuf, parser, ec);
        if(ec)
            return;
        dynabuf.consume(bytes_used);
    }
    while(! parser.is_complete());
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Fields>
void
read(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    message<isRequest, Body, Fields>& msg)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Fields>>::value,
            "Reader requirements not met");
    error_code ec;
    beast::http::read(stream, dynabuf, msg, ec);
    if(ec)
        throw system_error{ec};
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Fields>
void
read(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    message<isRequest, Body, Fields>& msg,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Fields>>::value,
            "Reader requirements not met");
    message_parser<isRequest, Body, Fields> p;
    beast::http::read(stream, dynabuf, p, ec);
    if(ec)
        return;
    msg = p.release();
}

} // http
} // beast

#endif
