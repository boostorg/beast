//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_READ_IPP
#define BOOST_BEAST_HTTP_IMPL_READ_IPP

#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/detail/read.hpp>
#include <boost/beast/core/detail/stream_algorithm.hpp>
#include <boost/asio/error.hpp>

namespace boost {
namespace beast {
namespace http {

namespace detail {

// The default maximum number of bytes to transfer in a single operation.
static std::size_t constexpr default_max_transfer_size = 65536;

template<
    class DynamicBuffer,
    bool isRequest, class Derived,
    class Condition>
static
std::size_t
parse_until(
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec,
    Condition cond)
{
    if(ec == net::error::eof)
    {
        if(parser.got_some())
        {
            // caller sees EOF on next read
            ec = {};
            parser.put_eof(ec);
            BOOST_ASSERT(ec || parser.is_done());
        }
        else
        {
            ec = error::end_of_stream;
        }
        return 0;
    }
    if(ec)
        return 0;
    if(parser.is_done())
        return 0;
    if(buffer.size() > 0)
    {
        auto const bytes_used =
            parser.put(buffer.data(), ec);
        // total = total + bytes_used; // VFALCO Can't do this in a condition
        buffer.consume(bytes_used);
        if(ec == http::error::need_more)
        {
            if(buffer.size() >= buffer.max_size())
            {
                ec = http::error::buffer_overflow;
                return 0;
            }
            ec = {};
        }
        else if(ec || cond())
        {
            return 0;
        }
    }
    return default_max_transfer_size;
}

// predicate is true on any forward parser progress
template<bool isRequest, class Derived>
struct read_some_condition
{
    basic_parser<isRequest, Derived>& parser;

    template<class DynamicBuffer>
    std::size_t
    operator()(error_code& ec, std::size_t,
        DynamicBuffer& buffer)
    {
        return detail::parse_until(
            buffer, parser, ec,
            []
            {
                return true;
            });
    }
};

// predicate is true when parser header is complete
template<bool isRequest, class Derived>
struct read_header_condition
{
    basic_parser<isRequest, Derived>& parser;

    template<class DynamicBuffer>
    std::size_t
    operator()(error_code& ec, std::size_t,
        DynamicBuffer& buffer)
    {
        return detail::parse_until(
            buffer, parser, ec,
            [this]
            {
                return parser.is_header_done();
            });
    }
};

// predicate is true when parser message is complete
template<bool isRequest, class Derived>
struct read_all_condition
{
    basic_parser<isRequest, Derived>& parser;

    template<class DynamicBuffer>
    std::size_t
    operator()(error_code& ec, std::size_t,
        DynamicBuffer& buffer)
    {
        return detail::parse_until(
            buffer, parser, ec,
            [this]
            {
                return parser.is_done();
            });
    }
};

//------------------------------------------------------------------------------

template<
    class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
    class Handler>
class read_msg_op
    : public net::coroutine
{
    using parser_type =
        parser<isRequest, Body, Allocator>;

    using message_type =
        typename parser_type::value_type;

    struct data
    {
        Stream& s;
        message_type& m;
        parser_type p;

        data(
            Handler const&,
            Stream& s_,
            message_type& m_)
            : s(s_)
            , m(m_)
            , p(std::move(m))
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_msg_op(read_msg_op&&) = default;
    read_msg_op(read_msg_op const&) = delete;

    template<class DeducedHandler>
    read_msg_op(
        Stream& s,
        DynamicBuffer& b,
        message_type& m,
        DeducedHandler&& h)
        : d_(std::forward<DeducedHandler>(h), s, m)
    {
        http::async_read(s, b, d_->p, std::move(*this));
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred);

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    using executor_type = net::associated_executor_t<
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(d_.handler());
    }

    executor_type
    get_executor() const noexcept
    {
        return net::get_associated_executor(
            d_.handler(), d_->s.get_executor());
    }

    friend
    void* asio_handler_allocate(
        std::size_t size, read_msg_op* op)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_msg_op* op)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_msg_op* op)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
        class Handler>
void
read_msg_op<Stream, DynamicBuffer,
    isRequest, Body, Allocator, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    auto& d = *d_;
    if(! ec)
        d.m = d.p.release();
    d_.invoke(ec, bytes_transferred);
}

} // detail

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read_some(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    return beast::detail::read(stream, buffer,
        detail::read_some_condition<
            isRequest, Derived>{parser}, ec);
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(
        is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    beast::detail::async_read(stream, buffer,
        detail::read_some_condition<
            isRequest, Derived>{parser}, std::move(
                init.completion_handler));
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read_header(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(false);
    return beast::detail::read(stream, buffer,
        detail::read_header_condition<
            isRequest, Derived>{parser}, ec);
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_header(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(
        is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    parser.eager(false);
    beast::detail::async_read(stream, buffer,
        detail::read_header_condition<
            isRequest, Derived>{parser}, std::move(
                init.completion_handler));
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    return beast::detail::read(stream, buffer,
        detail::read_all_condition<
            isRequest, Derived>{parser}, ec);
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(
        is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    parser.eager(true);
    beast::detail::async_read(stream, buffer,
        detail::read_all_condition<
            isRequest, Derived>{parser}, std::move(
                init.completion_handler));
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read(stream, buffer, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    error_code& ec)
{
    static_assert(
        is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    parser<isRequest, Body, Allocator> p(std::move(msg));
    p.eager(true);
    auto const bytes_transferred =
        read(stream, buffer, p.base(), ec);
    if(ec)
        return bytes_transferred;
    msg = p.release();
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    ReadHandler&& handler)
{
    static_assert(
        is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_msg_op<
        AsyncReadStream,
        DynamicBuffer,
        isRequest, Body, Allocator,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>(
                stream, buffer, msg, std::move(
                    init.completion_handler));
    return init.result.get();
}

} // http
} // beast
} // boost

#endif
