//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_BASIC_STREAM_SOCKET_HPP
#define BOOST_BEAST_CORE_IMPL_BASIC_STREAM_SOCKET_HPP

#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/assert.hpp>
#include <boost/core/exchange.hpp>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

template<class Protocol, class Executor>
struct basic_stream_socket<
    Protocol, Executor>::read_timeout_handler
{
    std::shared_ptr<impl_type> impl;

    void
    operator()(error_code ec)
    {
        // timer canceled
        if(ec == net::error::operation_aborted)
            return;

        BOOST_ASSERT(! ec);

        if(! impl->read_closed)
        {
            // timeout
            impl->close();
            impl->read_closed = true;
        }
        else
        {
            // late completion
            impl->read_closed = false;
        }
    }
};

template<class Protocol, class Executor>
struct basic_stream_socket<
    Protocol, Executor>::write_timeout_handler
{
    std::shared_ptr<impl_type> impl;

    void
    operator()(error_code ec)
    {
        // timer canceled
        if(ec == net::error::operation_aborted)
            return;

        BOOST_ASSERT(! ec);

        if(! impl->write_closed)
        {
            // timeout
            impl->close();
            impl->write_closed = true;
        }
        else
        {
            // late completion
            impl->write_closed = false;
        }
    }
};

/*
    The algorithm for implementing the timeout depends
    on the executor providing ordered execution guarantee.
    `net::strand` automatically provides this, and we assume
    that an implicit strand (one thread calling io_context::run)
    also provides this.
*/

template<class Protocol, class Executor>
template<class Buffers, class Handler>
class basic_stream_socket<Protocol, Executor>::read_op
    : public async_op_base<Handler, Executor>
    , public boost::asio::coroutine
{
    typename basic_stream_socket<
        Protocol, Executor>::impl_type& impl_;
    pending_guard pg_;
    Buffers b_;

public:
    template<class Handler_>
    read_op(
        basic_stream_socket& s,
        Buffers const& b,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            s.get_executor(), std::forward<Handler_>(h))
        , impl_(*s.impl_)
        , pg_(impl_.read_pending)
        , b_(b)
    {
        (*this)({});
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // must come first
            // VFALCO TODO what about the handler's allocator?
            impl_.read_timer.async_wait(
                net::bind_executor(
                    this->get_executor(),
                    read_timeout_handler{
                        impl_.shared_from_this()}));

            impl_.maybe_kick();

            // check if the balance is zero
            if(impl_.read_remain == 0)
            {
                // wait until the next time slice
                ++impl_.waiting;
                BOOST_ASIO_CORO_YIELD
                impl_.rate_timer.async_wait(std::move(*this));

                if(ec)
                {
                    // caused by impl::close on timeout
                    BOOST_ASSERT(
                        ec == net::error::operation_aborted);
                    goto upcall;
                }

                // must call this
                impl_.on_timer();
                BOOST_ASSERT(impl_.read_remain > 0);
            }

            // we always use buffers_prefix,
            // to reduce template instantiations.
            BOOST_ASSERT(impl_.read_remain > 0);
            BOOST_ASIO_CORO_YIELD
            impl_.socket.async_read_some(
                beast::buffers_prefix(
                    impl_.read_remain, b_),
                        std::move(*this));

            if(impl_.read_remain != no_limit)
            {
                // adjust balance
                BOOST_ASSERT(
                    bytes_transferred <= impl_.read_remain);
                impl_.read_remain -= bytes_transferred;
            }

            {
                // try cancelling timer
                auto const n =
                    impl_.read_timer.cancel();

                if(impl_.read_closed)
                {
                    // timeout handler already invoked
                    BOOST_ASSERT(n == 0);
                    ec = beast::error::timeout;
                    impl_.read_closed = false;
                }
                else if(n == 0)
                {
                    // timeout handler already queued
                    ec = beast::error::timeout;

                    impl_.close();
                    impl_.read_closed = true;
                }
                else
                {
                    // timeout was canceled
                    BOOST_ASSERT(n == 1);
                }
            }

        upcall:
            pg_.reset();
            this->invoke(ec, bytes_transferred);
        }
    }
};

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
template<class Buffers, class Handler>
class basic_stream_socket<Protocol, Executor>::write_op
    : public async_op_base<Handler, Executor>
    , public boost::asio::coroutine
{
    typename basic_stream_socket<
        Protocol, Executor>::impl_type& impl_;
    pending_guard pg_;
    Buffers b_;

public:
    template<class Handler_>
    write_op(
        basic_stream_socket& s,
        Buffers const& b,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            s.get_executor(), std::forward<Handler_>(h))
        , impl_(*s.impl_)
        , pg_(impl_.write_pending)
        , b_(b)
    {
        (*this)();
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // must come first
            // VFALCO TODO what about the handler's allocator?
            impl_.write_timer.async_wait(
                net::bind_executor(
                    this->get_executor(),
                    write_timeout_handler{
                        impl_.shared_from_this()}));

            impl_.maybe_kick();

            // check if the balance is zero
            if(impl_.write_remain == 0)
            {
                // wait until the next time slice
                ++impl_.waiting;
                BOOST_ASIO_CORO_YIELD
                impl_.rate_timer.async_wait(std::move(*this));

                if(ec)
                {
                    // caused by impl::close on timeout
                    BOOST_ASSERT(
                        ec == net::error::operation_aborted);
                    goto upcall;
                }

                // must call this
                impl_.on_timer();
                BOOST_ASSERT(impl_.write_remain > 0);
            }

            // we always use buffers_prefix,
            // to reduce template instantiations.
            BOOST_ASSERT(impl_.write_remain > 0);
            BOOST_ASIO_CORO_YIELD
            impl_.socket.async_write_some(
                beast::buffers_prefix(
                    impl_.write_remain, b_),
                        std::move(*this));

            if(impl_.write_remain != no_limit)
            {
                // adjust balance
                BOOST_ASSERT(
                    bytes_transferred <= impl_.write_remain);
                impl_.write_remain -= bytes_transferred;
            }

            {
                // try cancelling timer
                auto const n =
                    impl_.write_timer.cancel();

                if(impl_.write_closed)
                {
                    // timeout handler already invoked
                    BOOST_ASSERT(n == 0);
                    ec = beast::error::timeout;
                    impl_.write_closed = false;
                }
                else if(n == 0)
                {
                    // timeout handler already queued
                    ec = beast::error::timeout;

                    impl_.close();
                    impl_.write_closed = true;
                }
                else
                {
                    // timeout was canceled
                    BOOST_ASSERT(n == 1);
                }
            }

        upcall:
            pg_.reset();
            this->invoke(ec, bytes_transferred);
        }
    }
};

//------------------------------------------------------------------------------

namespace detail {

template<
    class Protocol, class Executor, class Handler>
class stream_socket_connect_op
    : public async_op_base<Handler, Executor>
{
    using stream_type =
        beast::basic_stream_socket<Protocol, Executor>;
    using timeout_handler =
        typename stream_type::write_timeout_handler;

    typename basic_stream_socket<
        Protocol, Executor>::impl_type& impl_;
    typename stream_type::pending_guard pg0_;
    typename stream_type::pending_guard pg1_;

public:
    template<
        class Endpoints, class Condition,
        class Handler_>
    stream_socket_connect_op(
        stream_type& s,
        Endpoints const& eps,
        Condition cond,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            s.get_executor(), std::forward<Handler_>(h))
        , impl_(*s.impl_)
        , pg0_(impl_.read_pending)
        , pg1_(impl_.write_pending)
    {
        // must come first
        // VFALCO TODO what about the handler's allocator?
        impl_.write_timer.async_wait(
            net::bind_executor(
                this->get_executor(),
                timeout_handler{
                    impl_.shared_from_this()}));

        net::async_connect(impl_.socket,
            eps, cond, std::move(*this));
        // *this is now moved-from
    }

    template<
        class Iterator, class Condition,
        class Handler_>
    stream_socket_connect_op(
        stream_type& s,
        Iterator begin, Iterator end,
        Condition cond,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            s.get_executor(), std::forward<Handler_>(h))
        , impl_(*s.impl_)
        , pg0_(impl_.read_pending)
        , pg1_(impl_.write_pending)
    {
        // must come first
        impl_.write_timer.async_wait(
            net::bind_executor(
                this->get_executor(),
                timeout_handler{
                    impl_.shared_from_this()}));

        net::async_connect(impl_.socket,
            begin, end, cond, std::move(*this));
        // *this is now moved-from
    }

    template<class Arg>
    void
    operator()(error_code ec, Arg&& arg)
    {
        // try to cancel the timer
        auto const n =
            impl_.write_timer.cancel();

        if(impl_.write_closed)
        {
            // timeout handler already invoked
            BOOST_ASSERT(n == 0);
            ec = beast::error::timeout;
            impl_.write_closed = false;
        }
        else if(n == 0)
        {
            // timeout handler already queued
            ec = beast::error::timeout;

            impl_.close();
            impl_.write_closed = true;
        }
        else
        {
            // timeout was canceled
            BOOST_ASSERT(n == 1);
        }

        pg0_.reset();
        pg1_.reset();
        this->invoke(ec, std::forward<Arg>(arg));
    }
};

} // detail

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
template<class... Args>
basic_stream_socket<Protocol, Executor>::
impl_type::
impl_type(
    Executor const& ex_,
    Args&&... args)
    : ex(ex_)
    , socket(std::forward<Args>(args)...)
    , rate_timer(ex.context())
    , read_timer(ex.context())
    , write_timer(ex.context())
{
    reset();
}

template<class Protocol, class Executor>
auto
basic_stream_socket<Protocol, Executor>::
impl_type::
operator=(impl_type&& other) -> impl_type&
{
    // VFALCO This hack is because legacy io_context::strand
    // doesn't support operator=. Don't worry, constructing
    // an executor cannot throw.
    ex.~Executor();
    ::new(&ex) Executor(other.ex);

    socket          = std::move(other.socket);
    rate_timer      = std::move(other.rate_timer);
    read_timer      = std::move(other.read_timer);
    write_timer     = std::move(other.write_timer);

    read_limit      = other.read_limit;
    read_remain     = other.read_remain;
    write_limit     = other.write_limit;
    write_remain    = other.write_remain;

    waiting         = other.waiting;
    read_pending    = other.read_pending;
    read_closed     = other.read_closed;
    write_pending   = other.write_pending;
    write_closed    = other.write_closed;

    return *this;
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
impl_type::
reset()
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(! read_pending || ! write_pending);

    if(! read_pending)
        BOOST_VERIFY(
            read_timer.expires_at(never()) == 0);

    if(! write_pending)
        BOOST_VERIFY(
            write_timer.expires_at(never()) == 0);
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
impl_type::
close()
{
    socket.close();
    rate_timer.cancel();
    read_timer.cancel();
    write_timer.cancel();
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
impl_type::
maybe_kick()
{
    // see if the timer needs a kick
    if(waiting > 0)
    {
        BOOST_ASSERT(
            rate_timer.expiry() != never());
        return;
    }

    // are both limits disabled?
    if( read_limit == no_limit &&
        write_limit == no_limit)
        return;

    BOOST_ASSERT(
        read_pending || write_pending);

    // update budget
    read_remain = read_limit;
    write_remain = write_limit;

    // start the clock
    ++waiting;
    on_timer();
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
impl_type::
on_timer()
{
    BOOST_ASSERT(waiting > 0);

    // the last waiter starts the new slice
    if(--waiting > 0)
        return;

    // update the expiration time
    BOOST_VERIFY(rate_timer.expires_after(
        std::chrono::seconds(rate_seconds)) == 0);

    // update budget
    read_remain = read_limit;
    write_remain = write_limit;

    // wait again
    ++waiting;
    auto const this_ = this->shared_from_this();
    rate_timer.async_wait(
        net::bind_executor(ex,
            [this_](error_code ec)
            {
                if(ec == net::error::operation_aborted)
                    return;
                BOOST_ASSERT(! ec);
                if(ec)
                    return;
                this_->on_timer();
            }
            ));
}

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
~basic_stream_socket()
{
    // the shared object can outlive *this,
    // cancel any operations so the shared
    // object is destroyed as soon as possible.
    impl_->close();
}

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(ExecutionContext& ctx)
    : impl_(std::make_shared<impl_type>(
        ctx.get_executor(),
        ctx))
{
    static_assert(
        std::is_same<ExecutionContext, net::io_context>::value,
        "Only net::io_context is currently supported for ExecutionContext");
}

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(executor_type const& ex)
    : impl_(std::make_shared<impl_type>(
        ex,
        ex.context()))
{
}

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    ExecutionContext& ctx,
    protocol_type const& protocol)
    : impl_(std::make_shared<impl_type>(
        ctx.get_executor(),
        ctx,
        protocol))
{
    static_assert(
        std::is_same<ExecutionContext, net::io_context>::value,
        "Only net::io_context is currently supported for ExecutionContext");
}

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    executor_type const& ex,
    protocol_type const& protocol)
    : impl_(std::make_shared<impl_type>(
        ex,
        ex.context(),
        protocol))
{
}

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    ExecutionContext& ctx,
    endpoint_type const& endpoint)
    : impl_(std::make_shared<impl_type>(
        ctx.get_executor(),
        ctx,
        endpoint))
{
    static_assert(
        std::is_same<ExecutionContext, net::io_context>::value,
        "Only net::io_context is currently supported for ExecutionContext");
}

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    executor_type const& ex,
    endpoint_type const& endpoint)
    : impl_(std::make_shared<impl_type>(
        ex,
        ex.context(),
        endpoint))
{
}

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    ExecutionContext& ctx,
    next_layer_type&& socket)
    : impl_(std::make_shared<impl_type>(
        ctx.get_executor(),
        std::move(socket)))
{
    static_assert(
        std::is_same<ExecutionContext, net::io_context>::value,
        "Only net::io_context is currently supported for ExecutionContext");
}

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    executor_type const& ex,
    next_layer_type&& socket)
    : impl_(std::make_shared<impl_type>(
        ex,
        std::move(socket)))
{
}

template<class Protocol, class Executor>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(basic_stream_socket&& other)
    : impl_(std::make_shared<impl_type>(
        std::move(*other.impl_)))
{
    // Can't move while operations are pending!
    BOOST_ASSERT(! impl_->read_pending);
    BOOST_ASSERT(! impl_->write_pending);
}

template<class Protocol, class Executor>
auto
basic_stream_socket<Protocol, Executor>::
operator=(basic_stream_socket&& other) ->
    basic_stream_socket&
{
    // Can't move while operations are pending!
    BOOST_ASSERT(! impl_->read_pending);
    BOOST_ASSERT(! impl_->write_pending);
    BOOST_ASSERT(! other.impl_->read_pending);
    BOOST_ASSERT(! other.impl_->write_pending);
    *impl_ = std::move(*other.impl_);
    return *this;
}

template<class Protocol, class Executor>
template<class OtherProtocol, class OtherExecutor, class>
basic_stream_socket<Protocol, Executor>::
basic_stream_socket(
    basic_stream_socket<OtherProtocol, OtherExecutor>&& other)
    : impl_(std::make_shared<impl_type>(
        other.get_executor(),
        std::move(other.impl_->socket)))
{
    static_assert(std::is_same<
        net::io_context,
        typename std::decay<decltype(
            other.get_executor().context())>::type>::value,
        "Only net::io_context& is currently supported for other.get_executor().context()");
}

template<class Protocol, class Executor>
template<class OtherProtocol, class OtherExecutor, class>
auto
basic_stream_socket<Protocol, Executor>::
operator=(
    basic_stream_socket<OtherProtocol, OtherExecutor>&& other) ->
        basic_stream_socket&
{
    static_assert(std::is_same<
        net::io_context,
        typename std::decay<decltype(
            other.get_executor().context())>::type>::value,
        "Only net::io_context& is currently supported for other.get_executor().context()");

    // Can't move while operations are pending!
    BOOST_ASSERT(! impl_->read_pending);
    BOOST_ASSERT(! impl_->write_pending);
    BOOST_ASSERT(! other.impl_->read_pending);
    BOOST_ASSERT(! other.impl_->write_pending);

    impl_ = std::make_shared<impl_type>(
        other.get_executor(),
        std::move(other.impl_->socket));
    return *this;
}

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
read_limit(std::size_t bytes_per_second)
{
    if(bytes_per_second == 0)
    {
        impl_->read_limit = no_limit;
    }
    else if(bytes_per_second < no_limit)
    {
        impl_->read_limit =
            bytes_per_second * rate_seconds;
    }
    else
    {
        impl_->read_limit = no_limit - 1;
    }
    BOOST_ASSERT(impl_->read_limit > 0);
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
write_limit(std::size_t bytes_per_second)
{
    if(bytes_per_second == 0)
    {
        impl_->write_limit = no_limit;
    }
    else if(bytes_per_second < no_limit)
    {
        impl_->write_limit =
            bytes_per_second * rate_seconds;
    }
    else
    {
        impl_->write_limit = no_limit - 1;
    }
    BOOST_ASSERT(impl_->write_limit > 0);
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
expires_after(std::chrono::nanoseconds expiry_time)
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(
        ! impl_->read_pending ||
        ! impl_->write_pending);

    if(! impl_->read_pending)
        BOOST_VERIFY(
            impl_->read_timer.expires_after(
                expiry_time) == 0);

    if(! impl_->write_pending)
        BOOST_VERIFY(
            impl_->write_timer.expires_after(
                expiry_time) == 0);
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
expires_at(
    net::steady_timer::time_point expiry_time)
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(
        ! impl_->read_pending ||
        ! impl_->write_pending);

    if(! impl_->read_pending)
        BOOST_VERIFY(
            impl_->read_timer.expires_at(
                expiry_time) == 0);

    if(! impl_->write_pending)
        BOOST_VERIFY(
            impl_->write_timer.expires_at(
                expiry_time) == 0);
}

template<class Protocol, class Executor>
void
basic_stream_socket<Protocol, Executor>::
expires_never()
{
    impl_->reset();
}

template<class Protocol, class Executor>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
    void(error_code, std::size_t))
basic_stream_socket<Protocol, Executor>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_op<MutableBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t))>(
            *this, buffers, std::forward<ReadHandler>(handler));
    return init.result.get();
}

template<class Protocol, class Executor>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
    void(error_code, std::size_t))
basic_stream_socket<Protocol, Executor>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    write_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler,void(error_code, std::size_t))>(
            *this, buffers, std::forward<WriteHandler>(handler));
    return init.result.get();
}

//------------------------------------------------------------------------------

namespace detail {

struct any_endpoint
{
    template<class Error, class Endpoint>
    bool
    operator()(
        Error const&, Endpoint const&) const noexcept
    {
        return true;
    }
};

} // detail

template<
    class Protocol, class Executor,
    class EndpointSequence,
    class RangeConnectHandler,
    class>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void(error_code, typename Protocol::endpoint))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    detail::stream_socket_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(error_code,
                typename Protocol::endpoint))>(
        s, endpoints, detail::any_endpoint{},
            std::forward<RangeConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class EndpointSequence,
    class ConnectCondition,
    class RangeConnectHandler,
    class>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void (error_code, typename Protocol::endpoint))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    detail::stream_socket_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(error_code,
                typename Protocol::endpoint))>(
        s, endpoints, connect_condition,
            std::forward<RangeConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    detail::stream_socket_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(error_code, Iterator))>(
        s, begin, end, detail::any_endpoint{},
            std::forward<IteratorConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class Iterator,
    class ConnectCondition,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    detail::stream_socket_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(error_code, Iterator))>(
        s, begin, end, connect_condition,
            std::forward<IteratorConnectHandler>(handler));
    return init.result.get();
}

} // beast
} // boost

#endif
