//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_IMPL_READ_HPP
#define BOOST_BEAST_DETAIL_IMPL_READ_HPP

#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/asio/basic_stream_socket.hpp>

namespace boost {
namespace beast {
namespace detail {

// The number of bytes in the stack buffer when using non-blocking.
static std::size_t constexpr default_max_stack_buffer = 16384;

//------------------------------------------------------------------------------

// read into a dynamic buffer until the
// condition is met or an error occurs
template<
    class Stream,
    class DynamicBuffer,
    class Condition,
    class Handler>
class read_op : public net::coroutine
{
    Stream& s_;
    net::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    DynamicBuffer& b_;
    Condition cond_;
    Handler h_;
    std::size_t total_ = 0;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = delete;

    template<class DeducedHandler>
    read_op(
        Stream& s,
        DynamicBuffer& b,
        Condition cond,
        DeducedHandler&& h)
        : s_(s)
        , wg_(s_.get_executor())
        , b_(b)
        , cond_(cond)
        , h_(std::forward<DeducedHandler>(h))
    {
        (*this)({}, 0, false);
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred,
        bool cont = true);

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    using executor_type = net::associated_executor_t<
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    executor_type
    get_executor() const noexcept
    {
        return net::get_associated_executor(
            h_, s_.get_executor());
    }

    friend
    void* asio_handler_allocate(
        std::size_t size, read_op* op)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_op* op)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }
    
    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<
    class Stream, class DynamicBuffer,
    class Condition, class Handler>
void
read_op<Stream, DynamicBuffer, Condition, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    std::size_t max_size;
    std::size_t max_prepare;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        max_size = cond_(ec, total_, b_);
        max_prepare = std::min<std::size_t>(
            std::max<std::size_t>(
                512, b_.capacity() - b_.size()),
            std::min<std::size_t>(
                max_size, b_.max_size() - b_.size()));
        while(max_prepare > 0)
        {
            BOOST_ASIO_CORO_YIELD
            s_.async_read_some(
                b_.prepare(max_prepare), std::move(*this));
            b_.commit(bytes_transferred);
            total_ += bytes_transferred;
            max_size = cond_(ec, total_, b_);
            max_prepare = std::min<std::size_t>(
                std::max<std::size_t>(
                    512, b_.capacity() - b_.size()),
                std::min<std::size_t>(
                    max_size, b_.max_size() - b_.size()));
        }
        if(! cont)
        {
            BOOST_ASIO_CORO_YIELD
            net::post(s_.get_executor(),
                beast::bind_front_handler(
                    std::move(*this), ec, total_));
        }
        h_(ec, total_);
    }
}

//------------------------------------------------------------------------------

#ifdef BOOST_BEAST_ENABLE_NON_BLOCKING
// EXPERIMENTAL
// optimized non-blocking read algorithm
template<
    class Protocol,
    class DynamicBuffer,
    class Condition,
    class Handler>
class read_non_blocking_op : public net::coroutine
{
    net::basic_stream_socket<Protocol>& s_;
    net::executor_work_guard<decltype(
        s_.get_executor())> wg_;
    DynamicBuffer& b_;
    Condition cond_;
    Handler h_;
    std::size_t limit_;
    std::size_t total_ = 0;

public:
    read_non_blocking_op(read_non_blocking_op&&) = default;
    read_non_blocking_op(read_non_blocking_op const&) = delete;

    template<class DeducedHandler>
    read_non_blocking_op(
        net::basic_stream_socket<Protocol>& s,
        DynamicBuffer& b,
        Condition cond,
        DeducedHandler&& h)
        : s_(s)
        , wg_(s_.get_executor())
        , b_(b)
        , cond_(cond)
        , h_(std::forward<DeducedHandler>(h))
    {
        (*this)({}, false);
    }

    void
    operator()(error_code ec, bool cont = true);

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    using executor_type = net::associated_executor_t<
        Handler, decltype(s_.get_executor())>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    executor_type
    get_executor() const noexcept
    {
        return net::get_associated_executor(
            h_, s_.get_executor());
    }

    friend
    void* asio_handler_allocate(
        std::size_t size, read_non_blocking_op* op)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(void* p,
        std::size_t size, read_non_blocking_op* op)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }
    
    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, read_non_blocking_op* op)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<
    class Protocol, class DynamicBuffer,
    class Condition, class Handler>
void
read_non_blocking_op<
    Protocol, DynamicBuffer, Condition, Handler>::
operator()(error_code ec, bool cont)
{
    std::size_t n;
    std::size_t bytes_transferred;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        limit_ = cond_(ec, total_, b_);
        for(;;)
        {
            n = detail::min<std::size_t>(
                limit_, b_.max_size() - b_.size());
            if(n == 0)
                break;
            BOOST_ASIO_CORO_YIELD
            s_.async_wait(
                net::socket_base::wait_read, std::move(*this));
            if(b_.size() <= default_max_stack_buffer)
            {
                flat_static_buffer<
                    default_max_stack_buffer> sb;
                bytes_transferred = net::buffer_copy(
                    sb.prepare(b_.size()), b_.data());
                sb.commit(bytes_transferred);
                b_.consume(bytes_transferred);
                //detail::shrink_to_fit(b_);
                n = detail::min<std::size_t>(
                    limit_,
                    sb.capacity() - sb.size(),
                    b_.max_size() - sb.size());
                BOOST_ASSERT(n > 0);
                bytes_transferred =
                    s_.read_some(sb.prepare(n), ec);
                sb.commit(bytes_transferred);
                total_ += bytes_transferred;
                limit_ = cond_(ec, total_, sb);
                b_.commit(net::buffer_copy(
                    b_.prepare(sb.size()), sb.data()));
            }
            else
            {
                n = detail::min<std::size_t>(
                    limit_,
                    s_.available(),
                    b_.max_size() - b_.size(),
                    std::max<std::size_t>(
                        512, b_.capacity() - b_.size()));
                BOOST_ASSERT(n > 0);
                bytes_transferred = s_.read_some(
                    b_.prepare(n), ec);
                b_.commit(bytes_transferred);
                total_ += bytes_transferred;
                limit_ = cond_(ec, total_, b_);
            }
        }
        if(! cont)
        {
            BOOST_ASIO_CORO_YIELD
            net::post(s_.get_executor(),
                beast::bind_front_handler(
                    std::move(*this), ec, total_));
        }
        h_(ec, total_);
    }
}
#endif

//------------------------------------------------------------------------------

template<
    class AsyncReadStream,
    class DynamicBuffer,
    class CompletionCondition,
    class ReadHandler,
    class>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    CompletionCondition cond,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(
        detail::is_invocable<CompletionCondition,
            void(error_code&, std::size_t, DynamicBuffer&)>::value,
        "CompletionCondition requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_op<
        AsyncReadStream,
        DynamicBuffer,
        CompletionCondition,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>(
        stream, buffer, std::move(cond),
            std::move(init.completion_handler));
    return init.result.get();
}

#ifdef BOOST_BEAST_ENABLE_NON_BLOCKING
template<
    class Protocol,
    class DynamicBuffer,
    class CompletionCondition,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    net::basic_stream_socket<Protocol>& socket,
    DynamicBuffer& buffer,
    CompletionCondition cond,
    ReadHandler&& handler)
{
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(
        detail::is_invocable<CompletionCondition,
            void(error_code&, std::size_t, DynamicBuffer&)>::value,
        "CompletionCondition requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    if(socket.non_blocking())
    {
        detail::read_non_blocking_op<
            Protocol,
            DynamicBuffer,
            CompletionCondition,
            BOOST_ASIO_HANDLER_TYPE(
                ReadHandler, void(error_code, std::size_t))>(
            socket, buffer, std::move(cond),
                std::move(init.completion_handler));
    }
    else
    {
        detail::read_op<
            decltype(socket),
            DynamicBuffer,
            CompletionCondition,
            BOOST_ASIO_HANDLER_TYPE(
                ReadHandler, void(error_code, std::size_t))>(
            socket, buffer, std::move(cond),
                std::move(init.completion_handler));
    }
    return init.result.get();
}
#endif

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    class CompletionCondition,
    class>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    CompletionCondition cond)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(
        detail::is_invocable<CompletionCondition,
            void(error_code&, std::size_t, DynamicBuffer&)>::value,
        "CompletionCondition requirements not met");
    error_code ec;
    auto const bytes_transferred = read(
        stream, buffer, std::move(cond), ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    class CompletionCondition,
    class>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    CompletionCondition cond,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(
        detail::is_invocable<CompletionCondition,
            void(error_code&, std::size_t, DynamicBuffer&)>::value,
        "CompletionCondition requirements not met");
    ec = {};
    std::size_t total = 0;
    std::size_t max_size;
    std::size_t max_prepare;
    max_size = cond(ec, total, buffer);
    max_prepare = std::min<std::size_t>(
        std::max<std::size_t>(
            512, buffer.capacity() - buffer.size()),
        std::min<std::size_t>(
            max_size, buffer.max_size() - buffer.size()));
    while(max_prepare > 0)
    {
        std::size_t const bytes_transferred =
            stream.read_some(buffer.prepare(max_prepare), ec);
        buffer.commit(bytes_transferred);
        total += bytes_transferred;
        max_size = cond(ec, total, buffer);
        max_prepare = std::min<std::size_t>(
            std::max<std::size_t>(
                512, buffer.capacity() - buffer.size()),
            std::min<std::size_t>(
                max_size, buffer.max_size() - buffer.size()));
    }
    return total;
}

#ifdef BOOST_BEAST_ENABLE_NON_BLOCKING
template<
    class Protocol,
    class DynamicBuffer,
    class CompletionCondition>
std::size_t
read(
    net::basic_stream_socket<Protocol>& socket,
    DynamicBuffer& buffer,
    CompletionCondition cond,
    error_code& ec)
{
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(
        detail::is_invocable<CompletionCondition,
            void(error_code&, std::size_t, DynamicBuffer&)>::value,
        "CompletionCondition requirements not met");
    ec = {};
    std::size_t n;
    std::size_t limit;
    std::size_t total = 0;
    std::size_t bytes_transferred;
    limit = cond(ec, total, buffer);
    for(;;)
    {
        n = detail::min<std::size_t>(
            limit, buffer.max_size() - buffer.size());
        if(n == 0)
            break;
        socket.non_blocking(false);
        socket.wait(net::socket_base::wait_read, ec);
        socket.non_blocking(true);
        if(ec)
        {
            limit = cond(ec, total, buffer);
        }
        else if(buffer.size() <= default_max_stack_buffer)
        {
            flat_static_buffer<
                default_max_stack_buffer> sb;
            bytes_transferred = net::buffer_copy(
                sb.prepare(buffer.size()), buffer.data());
            sb.commit(bytes_transferred);
            buffer.consume(bytes_transferred);
            //detail::shrink_to_fit(buffer);
            n = detail::min<std::size_t>(
                limit,
                sb.capacity() - sb.size(),
                buffer.max_size() - sb.size());
            BOOST_ASSERT(n > 0);
            bytes_transferred =
                socket.read_some(sb.prepare(n), ec);
            if(ec != net::error::would_block)
            {
                sb.commit(bytes_transferred);
                total += bytes_transferred;
                limit = cond(ec, total, sb);
            }
            buffer.commit(net::buffer_copy(
                buffer.prepare(sb.size()), sb.data()));
        }
        else
        {
            n = detail::min<std::size_t>(
                limit,
                socket.available(),
                buffer.max_size() - buffer.size(),
                std::max<std::size_t>(
                    512, buffer.capacity() - buffer.size()));
            BOOST_ASSERT(n > 0);
            bytes_transferred = socket.read_some(
                buffer.prepare(n), ec);
            if(ec != net::error::would_block)
            {
                buffer.commit(bytes_transferred);
                total += bytes_transferred;
                limit = cond(ec, total, buffer);
            }
        }
    }
    return total;
}
#endif

} // detail
} // beast
} // boost

#endif
