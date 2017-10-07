//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_READ_IPP_HPP
#define BOOST_BEAST_HTTP_IMPL_READ_IPP_HPP

#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/coroutine.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>

namespace boost {
namespace beast {
namespace http {

namespace detail {

//------------------------------------------------------------------------------

template<typename DynamicBuffer>
boost::optional<typename DynamicBuffer::mutable_buffers_type>
prepare_or_error(DynamicBuffer& buffer, error_code& ec)
{
    try
    {
        ec.assign(0, ec.category());
        return {buffer.prepare(read_size_or_throw(buffer, 65536))};
    }
    catch(const std::length_error& error)
    {
        ec = error::buffer_overflow;
        return {};
    }
}

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
class read_some_op : asio::coroutine
{
    Stream& s_;
    DynamicBuffer& b_;
    basic_parser<isRequest, Derived>& p_;
    std::size_t total_bytes_consumed_ = 0;
    Handler h_;

public:
    read_some_op(read_some_op&&) = default;
    read_some_op(read_some_op const&) = default;

    template<class DeducedHandler>
    read_some_op(DeducedHandler&& h, Stream& s,
        DynamicBuffer& b, basic_parser<isRequest, Derived>& p)
        : s_(s)
        , b_(b)
        , p_(p)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_some_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_some_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(read_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return is_continuation(*op) ||
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }

    void do_upcall(error_code ec) const
    {
        s_.get_io_service().post(
            bind_handler(std::move(*this), ec, 0));
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
void
read_some_op<Stream, DynamicBuffer,
    isRequest, Derived, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    // Temporary variables, not to be used across yields.
    // They have to be declared here due to initialization & yield issues.
    size_t consumed = 0;
    boost::optional<typename
        DynamicBuffer::mutable_buffers_type> mb;

    BOOST_ASIO_CORO_REENTER(*this)
    {
        if (b_.size() > 0)
        {
            consumed = p_.put(b_.data(), ec);
            total_bytes_consumed_ += consumed;
            b_.consume(consumed);
            if (ec == http::error::need_more)
            {
                ec.assign(0, ec.category());
            }
            BOOST_ASIO_CORO_YIELD do_upcall(ec);
        }
        while (!ec && total_bytes_consumed_ == 0)
        {
            mb = prepare_or_error(b_, ec);
            if (ec)
            {
                BOOST_ASIO_CORO_YIELD do_upcall(ec);
                break;
            }

            BOOST_ASIO_CORO_YIELD s_.async_read_some(*mb, std::move(*this));
            BOOST_ASSERT(ec != asio::error::eof || bytes_transferred == 0);
            if (ec == asio::error::eof)
            {
                if (p_.got_some())
                {
                    p_.put_eof(ec);
                    BOOST_ASSERT(ec || p_.is_done());
                }
                else
                {
                    ec = error::end_of_stream;
                }
                break;
            }
            if (ec)
            {
                // No need to call do_upcall() - we're already being executed
                // within the io_service's context.
                break;
            }
            b_.commit(bytes_transferred);
            consumed = p_.put(b_.data(), ec);
            total_bytes_consumed_ += consumed;
            b_.consume(consumed);
            if (ec == http::error::need_more)
            {
                ec.assign(0, ec.category());
            }
        }
        h_(ec, total_bytes_consumed_);
    }
}

//------------------------------------------------------------------------------

struct parser_is_done
{
    template<bool isRequest, class Derived>
    bool
    operator()(basic_parser<
        isRequest, Derived> const& p) const
    {
        return p.is_done();
    }
};

struct parser_is_header_done
{
    template<bool isRequest, class Derived>
    bool
    operator()(basic_parser<
        isRequest, Derived> const& p) const
    {
        return p.is_header_done();
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Condition,
        class Handler>
class read_op : asio::coroutine
{
    Stream& s_;
    DynamicBuffer& b_;
    basic_parser<isRequest, Derived>& p_;
    std::size_t bytes_transferred_ = 0;
    Handler h_;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = default;

    template<class DeducedHandler>
    read_op(DeducedHandler&& h, Stream& s,
        DynamicBuffer& b, basic_parser<isRequest,
            Derived>& p)
        : s_(s)
        , b_(b)
        , p_(p)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return is_continuation(*op) ||
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Condition,
        class Handler>
void
read_op<Stream, DynamicBuffer,
    isRequest, Derived, Condition, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if (Condition{}(p_))
        {
            BOOST_ASIO_CORO_YIELD s_.get_io_service().post(
                bind_handler(std::move(*this), ec));
            h_(ec, bytes_transferred_);
            BOOST_ASIO_CORO_YIELD break;
        }
        do
        {
            BOOST_ASIO_CORO_YIELD async_read_some(
                s_, b_, p_, std::move(*this));
            bytes_transferred_ += bytes_transferred;
        } while (!Condition{}(p_) && !ec);
        h_(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
        class Handler>
class read_msg_op
{
    using parser_type =
        parser<isRequest, Body, Allocator>;

    using message_type =
        typename parser_type::value_type;

    struct data : boost::asio::coroutine
    {
        Stream& s;
        DynamicBuffer& b;
        message_type& m;
        parser_type p;
        std::size_t bytes_transferred = 0;

        data(Handler&, Stream& s_,
                DynamicBuffer& b_, message_type& m_)
            : s(s_)
            , b(b_)
            , m(m_)
            , p(std::move(m))
        {
            p.eager(true);
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_msg_op(read_msg_op&&) = default;
    read_msg_op(read_msg_op const&) = default;

    template<class DeducedHandler, class... Args>
    read_msg_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_msg_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_msg_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(read_msg_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return is_continuation(*op->d_) ||
            asio_handler_is_continuation(
                std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_msg_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
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
    BOOST_ASIO_CORO_REENTER(d)
    {
        while (!d.p.is_done())
        {
            BOOST_ASIO_CORO_YIELD async_read_some(
                d.s, d.b, d.p, std::move(*this));
            if (ec)
            {
                break;
            }
            d.bytes_transferred += bytes_transferred;

        }
    }
    if (d.is_complete())
    {
        // Can't call the handler within the coroutine body
        // because the data struct would be destroyed before
        // we left the coroutine, causing use-after-free.
        d_.invoke(ec, bytes_transferred);
    }
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    std::size_t bytes_transferred = 0;
    if(buffer.size() == 0)
        goto do_read;
    for(;;)
    {
        // invoke parser
        {
            auto const n = parser.put(buffer.data(), ec);
            bytes_transferred += n;
            buffer.consume(n);
            if(! ec)
                break;
            if(ec != http::error::need_more)
                break;
        }
    do_read:
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> b;
        try
        {
            b.emplace(buffer.prepare(
                read_size_or_throw(buffer, 65536)));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return bytes_transferred;
        }
        auto const n = stream.read_some(*b, ec);
        if(ec == boost::asio::error::eof)
        {
            BOOST_ASSERT(n == 0);
            if(parser.got_some())
            {
                // caller sees EOF on next read
                parser.put_eof(ec);
                if(ec)
                    break;
                BOOST_ASSERT(parser.is_done());
                break;
            }
            ec = error::end_of_stream;
            break;
        }
        if(ec)
            break;
        buffer.commit(n);
    }
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
async_return_type<
    ReadHandler, void(error_code, std::size_t)>
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    async_completion<ReadHandler, void(error_code)> init{handler};
    detail::read_some_op<AsyncReadStream,
        DynamicBuffer, isRequest, Derived, handler_type<
            ReadHandler, void(error_code, std::size_t)>>{
                init.completion_handler, stream, buffer, parser}();
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(false);
    if(parser.is_header_done())
    {
        ec.assign(0, ec.category());
        return 0;
    }
    std::size_t bytes_transferred = 0;
    do
    {
        bytes_transferred += read_some(
            stream, buffer, parser, ec);
        if(ec)
            return bytes_transferred;
    }
    while(! parser.is_header_done());
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
async_return_type<
    ReadHandler,
    void(error_code, std::size_t)>
async_read_header(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(false);
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    detail::read_op<AsyncReadStream, DynamicBuffer,
        isRequest, Derived, detail::parser_is_header_done,
            handler_type<ReadHandler, void(error_code, std::size_t)>>{
                init.completion_handler, stream, buffer, parser}();
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    if(parser.is_done())
    {
        ec.assign(0, ec.category());
        return 0;
    }
    std::size_t bytes_transferred = 0;
    do
    {
        bytes_transferred += read_some(
            stream, buffer, parser, ec);
        if(ec)
            return bytes_transferred;
    }
    while(! parser.is_done());
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
async_return_type<
    ReadHandler,
    void(error_code, std::size_t)>
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    async_completion<
        ReadHandler,
        void(error_code, std::size_t)> init{handler};
    detail::read_op<AsyncReadStream, DynamicBuffer,
        isRequest, Derived, detail::parser_is_done,
            handler_type<ReadHandler, void(error_code, std::size_t)>>{
                init.completion_handler, stream, buffer, parser}();
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
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
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    parser<isRequest, Body, Allocator> p{std::move(msg)};
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
async_return_type<
    ReadHandler,
    void(error_code, std::size_t)>
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    async_completion<
        ReadHandler,
        void(error_code, std::size_t)> init{handler};
    detail::read_msg_op<
        AsyncReadStream,
        DynamicBuffer,
        isRequest, Body, Allocator,
        handler_type<ReadHandler,
            void(error_code, std::size_t)>>{
                init.completion_handler, stream, buffer, msg}();
    return init.result.get();
}

} // http
} // beast
} // boost

#endif
