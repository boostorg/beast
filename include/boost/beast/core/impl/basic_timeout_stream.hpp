//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_BASIC_TIMEOUT_STREAM_HPP
#define BOOST_BEAST_CORE_IMPL_BASIC_TIMEOUT_STREAM_HPP

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
struct basic_timeout_stream<
    Protocol, Executor>::timeout_handler
{
    op_state& state;
    std::weak_ptr<impl_type> wp;
    tick_type tick;

    void
    operator()(error_code ec)
    {
        // timer canceled
        if(ec == net::error::operation_aborted)
            return;
        BOOST_ASSERT(! ec);

        auto sp = wp.lock();

        // stream destroyed
        if(! sp)
            return;

        // stale timer
        if(tick < state.tick)
            return;
        BOOST_ASSERT(tick == state.tick);

        // late completion
        if(state.timeout)
        {
            state.timeout = false;
            return;
        }

        // timeout
        sp->close();
        state.timeout = true;
    }
};

//------------------------------------------------------------------------------

/*
    The algorithm for implementing the timeout depends
    on the executor providing ordered execution guarantee.
    `net::strand` automatically provides this, and we assume
    that an implicit strand (one thread calling io_context::run)
    also provides this.
*/

template<class Protocol, class Executor>
template<bool isRead, class Buffers, class Handler>
class basic_timeout_stream<Protocol, Executor>::async_op
    : public async_op_base<Handler, Executor>
    , public boost::asio::coroutine
{
    std::shared_ptr<impl_type> impl_;
    pending_guard pg_;
    Buffers b_;

    op_state&
    state(std::true_type)
    {
        return impl_->read;
    }

    op_state&
    state(std::false_type)
    {
        return impl_->write;
    }

    op_state&
    state()
    {
        return state(
            std::integral_constant<bool, isRead>{});
    }

    void
    async_perform(std::true_type)
    {
        impl_->socket.async_read_some(
            b_, std::move(*this));
    }

    void
    async_perform(std::false_type)
    {
        impl_->socket.async_write_some(
            b_, std::move(*this));
    }

public:
    template<class Handler_>
    async_op(
        basic_timeout_stream& s,
        Buffers const& b,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            std::forward<Handler_>(h), s.get_executor())
        , impl_(s.impl_)
        , pg_(state().pending)
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
            // VFALCO TODO handle buffer size == 0

            state().timer.async_wait(
                net::bind_executor(
                    this->get_executor(),
                    timeout_handler{
                        state(),
                        impl_,
                        state().tick
                    }));

            BOOST_ASIO_CORO_YIELD
            async_perform(
                std::integral_constant<bool, isRead>{});
            ++state().tick;

            // try cancelling timer
            auto const n =
                state().timer.cancel();
            if(n == 0)
            {
                if(state().timeout)
                {
                    // timeout handler invoked
                    ec = beast::error::timeout;
                    state().timeout = false;
                }
                else
                {
                    // timeout handler queued, stale
                }
            }
            else
            {
                BOOST_ASSERT(n == 1);
                BOOST_ASSERT(! state().timeout);
            }

            pg_.reset();
            this->invoke(ec, bytes_transferred);
        }
    }
};

//------------------------------------------------------------------------------

namespace detail {

template<
    class Protocol, class Executor, class Handler>
class timeout_stream_connect_op
    : public async_op_base<Handler, Executor>
{
    using stream_type =
        beast::basic_timeout_stream<Protocol, Executor>;

    using timeout_handler =
        typename stream_type::timeout_handler;

    std::shared_ptr<typename basic_timeout_stream<
        Protocol, Executor>::impl_type> impl_;
    typename stream_type::pending_guard pg0_;
    typename stream_type::pending_guard pg1_;

    typename stream_type::op_state&
    state() noexcept
    {
        return impl_->write;
    }

public:
    template<
        class Endpoints, class Condition,
        class Handler_>
    timeout_stream_connect_op(
        stream_type& s,
        Endpoints const& eps,
        Condition cond,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            std::forward<Handler_>(h), s.get_executor())
        , impl_(s.impl_)
        , pg0_(impl_->read.pending)
        , pg1_(impl_->write.pending)
    {
        // must come first
        impl_->write.timer.async_wait(
            net::bind_executor(
                this->get_executor(),
                timeout_handler{
                    state(),
                    impl_,
                    state().tick}));

        net::async_connect(impl_->socket,
            eps, cond, std::move(*this));
        // *this is now moved-from
    }

    template<
        class Iterator, class Condition,
        class Handler_>
    timeout_stream_connect_op(
        stream_type& s,
        Iterator begin, Iterator end,
        Condition cond,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            std::forward<Handler_>(h), s.get_executor())
        , impl_(s.impl_)
        , pg0_(impl_->read.pending)
        , pg1_(impl_->write.pending)
    {
        // must come first
        impl_->write.timer.async_wait(
            net::bind_executor(
                this->get_executor(),
                timeout_handler{
                    state(),
                    impl_,
                    state().tick}));

        net::async_connect(impl_->socket,
            begin, end, cond, std::move(*this));
        // *this is now moved-from
    }

    template<class Handler_>
    timeout_stream_connect_op(
        stream_type& s,
        typename stream_type::endpoint_type ep,
        Handler_&& h)
        : async_op_base<Handler, Executor>(
            std::forward<Handler_>(h), s.get_executor())
        , impl_(s.impl_)
        , pg0_(impl_->read.pending)
        , pg1_(impl_->write.pending)
    {
        // must come first
        impl_->write.timer.async_wait(
            net::bind_executor(
                this->get_executor(),
                timeout_handler{
                    state(),
                    impl_,
                    state().tick}));

        impl_->socket.async_connect(
            ep, std::move(*this));
        // *this is now moved-from
    }

    template<class... Args>
    void
    operator()(error_code ec, Args&&... args)
    {
        ++state().tick;

        // try cancelling timer
        auto const n =
            impl_->write.timer.cancel();
        if(n == 0)
        {
            if(state().timeout)
            {
                // timeout handler invoked
                ec = beast::error::timeout;
                state().timeout = false;
            }
            else
            {
                // timeout handler queued, stale
            }
        }
        else
        {
            BOOST_ASSERT(n == 1);
            BOOST_ASSERT(! state().timeout);
        }
        
        pg0_.reset();
        pg1_.reset();
        this->invoke(ec, std::forward<Args>(args)...);
    }
};

} // detail

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
template<class... Args>
basic_timeout_stream<Protocol, Executor>::
impl_type::
impl_type(
    Executor const& ex_,
    Args&&... args)
    : ex(ex_)
    , read(ex_.context())
    , write(ex_.context())
    , socket(std::forward<Args>(args)...)
{
    reset();
}

template<class Protocol, class Executor>
auto
basic_timeout_stream<Protocol, Executor>::
impl_type::
operator=(impl_type&& other) -> impl_type&
{
    // VFALCO This hack is because legacy io_context::strand
    // doesn't support operator=. Don't worry, constructing
    // an executor cannot throw.
    ex.~Executor();
    ::new(&ex) Executor(other.ex);

    socket          = std::move(other.socket);
    read            = std::move(other.read);
    write           = std::move(other.write);

    return *this;
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
impl_type::
reset()
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(! read.pending || ! write.pending);

    if(! read.pending)
        BOOST_VERIFY(
            read.timer.expires_at(never()) == 0);

    if(! write.pending)
        BOOST_VERIFY(
            write.timer.expires_at(never()) == 0);
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
impl_type::
close()
{
    socket.close();

    // have to let the read/write ops cancel the timer,
    // otherwise we will get error::timeout on close when
    // we actually want net::error::operation_aborted.
    //
    //read.timer.cancel();
    //write.timer.cancel();
}

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
basic_timeout_stream<Protocol, Executor>::
~basic_timeout_stream()
{
    // the shared object can outlive *this,
    // cancel any operations so the shared
    // object is destroyed as soon as possible.
    impl_->close();
}

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_timeout_stream<Protocol, Executor>::
basic_timeout_stream(ExecutionContext& ctx)
    : impl_(std::make_shared<impl_type>(
        ctx.get_executor(),
        ctx))
{
    // Restriction is necessary until Asio fully supports P1322R0
    static_assert(
        std::is_same<ExecutionContext, net::io_context>::value,
        "Only net::io_context is currently supported for ExecutionContext");
}

template<class Protocol, class Executor>
basic_timeout_stream<Protocol, Executor>::
basic_timeout_stream(executor_type const& ex)
    : impl_(std::make_shared<impl_type>(
        ex, ex.context()))
{
}

template<class Protocol, class Executor>
basic_timeout_stream<Protocol, Executor>::
basic_timeout_stream(
    net::basic_stream_socket<Protocol>&& socket)
    : impl_(std::make_shared<impl_type>(
        socket.get_executor(), std::move(socket)))
{
}

template<class Protocol, class Executor>
basic_timeout_stream<Protocol, Executor>::
basic_timeout_stream(
    executor_type const& ex,
    net::basic_stream_socket<Protocol>&& socket)
    : impl_(std::make_shared<impl_type>(
        ex, std::move(socket)))
{
    // Restriction is necessary until Asio fully supports P1322R0
    if(ex.context().get_executor() != socket.get_executor())
        throw std::invalid_argument(
            "basic_timeout_stream currently requires ctx.get_executor() == socket.get_executor()");
}

template<class Protocol, class Executor>
basic_timeout_stream<Protocol, Executor>::
basic_timeout_stream(basic_timeout_stream&& other)
    : impl_(std::make_shared<impl_type>(
        std::move(*other.impl_)))
{
    // Can't move while operations are pending!
    BOOST_ASSERT(! impl_->read.pending);
    BOOST_ASSERT(! impl_->write.pending);
}

template<class Protocol, class Executor>
auto
basic_timeout_stream<Protocol, Executor>::
operator=(basic_timeout_stream&& other) ->
    basic_timeout_stream&
{
    // Can't move while operations are pending!
    BOOST_ASSERT(! impl_->read.pending);
    BOOST_ASSERT(! impl_->write.pending);
    BOOST_ASSERT(! other.impl_->read.pending);
    BOOST_ASSERT(! other.impl_->write.pending);
    *impl_ = std::move(*other.impl_);
    return *this;
}

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
expires_after(std::chrono::nanoseconds expiry_time)
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(
        ! impl_->read.pending ||
        ! impl_->write.pending);

    if(! impl_->read.pending)
        BOOST_VERIFY(
            impl_->read.timer.expires_after(
                expiry_time) == 0);

    if(! impl_->write.pending)
        BOOST_VERIFY(
            impl_->write.timer.expires_after(
                expiry_time) == 0);
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
expires_at(
    net::steady_timer::time_point expiry_time)
{
    // If assert goes off, it means that there are
    // already read or write (or connect) operations
    // outstanding, so there is nothing to apply
    // the expiration time to!
    //
    BOOST_ASSERT(
        ! impl_->read.pending ||
        ! impl_->write.pending);

    if(! impl_->read.pending)
        BOOST_VERIFY(
            impl_->read.timer.expires_at(
                expiry_time) == 0);

    if(! impl_->write.pending)
        BOOST_VERIFY(
            impl_->write.timer.expires_at(
                expiry_time) == 0);
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
expires_never()
{
    impl_->reset();
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
cancel()
{
    error_code ec;
    impl_->socket.cancel(ec);
}

template<class Protocol, class Executor>
void
basic_timeout_stream<Protocol, Executor>::
close()
{
    impl_->close();
}

template<class Protocol, class Executor>
template<class ConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
    void(error_code))
basic_timeout_stream<Protocol, Executor>::
async_connect(
    endpoint_type ep,
    ConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(
        ConnectHandler, void(error_code));
    detail::timeout_stream_connect_op<
        Protocol, Executor, BOOST_ASIO_HANDLER_TYPE(
            ConnectHandler, void(error_code))>(*this,
                ep, std::forward<ConnectHandler>(handler));
    return init.result.get();
}

template<class Protocol, class Executor>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
    void(error_code, std::size_t))
basic_timeout_stream<Protocol, Executor>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    async_op<true, MutableBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t))>(
            *this, buffers, std::move(init.completion_handler));
    return init.result.get();
}

template<class Protocol, class Executor>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
    void(error_code, std::size_t))
basic_timeout_stream<Protocol, Executor>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    async_op<false, ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>(
            *this, buffers, std::move(init.completion_handler));
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class Protocol, class Executor,
    class EndpointSequence,
    class RangeConnectHandler,
    class>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void(error_code, typename Protocol::endpoint))
async_connect(
    basic_timeout_stream<Protocol, Executor>& stream,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    detail::timeout_stream_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(error_code, typename Protocol::endpoint))>(
        stream, endpoints, detail::any_endpoint{},
            std::move(init.completion_handler));
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
    basic_timeout_stream<Protocol, Executor>& stream,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    detail::timeout_stream_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(error_code, typename Protocol::endpoint))>(
        stream, endpoints, connect_condition,
            std::move(init.completion_handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    basic_timeout_stream<Protocol, Executor>& stream,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    detail::timeout_stream_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(error_code, Iterator))>(
        stream, begin, end, detail::any_endpoint{},
            std::move(init.completion_handler));
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
    basic_timeout_stream<Protocol, Executor>& stream,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    detail::timeout_stream_connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(error_code, Iterator))>(
        stream, begin, end, connect_condition,
            std::move(init.completion_handler));
    return init.result.get();
}

} // beast
} // boost

#endif
