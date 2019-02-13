//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_PING_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_PING_HPP

#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/bind_continuation.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/impl/stream_impl.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*
    This composed operation handles sending ping and pong frames.
    It only sends the frames it does not make attempts to read
    any frame data.
*/
template<class NextLayer, bool deflateSupported>
template<class Handler>
class stream<NextLayer, deflateSupported>::ping_op
    : public beast::stable_async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    boost::weak_ptr<impl_type> wp_;
    detail::frame_buffer& fb_;

public:
    static constexpr int id = 3; // for soft_mutex

    template<class Handler_>
    ping_op(
        Handler_&& h,
        boost::shared_ptr<impl_type> const& sp,
        detail::opcode op,
        ping_data const& payload)
        : stable_async_op_base<Handler,
            beast::executor_type<stream>>(
                std::forward<Handler_>(h),
                    sp->stream.get_executor())
        , wp_(sp)
        , fb_(beast::allocate_stable<
            detail::frame_buffer>(*this))
    {
        // Serialize the ping or pong frame
        sp->template write_ping<
            flat_static_buffer_base>(fb_, op, payload);
        (*this)({}, 0, false);
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true)
    {
        boost::ignore_unused(bytes_transferred);
        auto sp = wp_.lock();
        if(! sp)
            return this->invoke(cont,
                net::error::operation_aborted);
        auto& impl = *sp;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Acquire the write lock
            if(! impl.wr_block.try_lock(this))
            {
                BOOST_ASIO_CORO_YIELD
                impl.op_ping.emplace(std::move(*this));
                impl.wr_block.lock(this);
                BOOST_ASIO_CORO_YIELD
                net::post(std::move(*this));
                BOOST_ASSERT(impl.wr_block.is_locked(this));
            }
            if(impl.check_stop_now(ec))
                goto upcall;

            // Send ping frame
            BOOST_ASIO_CORO_YIELD
            net::async_write(impl.stream, fb_.data(),
                beast::detail::bind_continuation(std::move(*this)));
            if(impl.check_stop_now(ec))
                goto upcall;

        upcall:
            impl.wr_block.unlock(this);
            impl.op_close.maybe_invoke()
                || impl.op_idle_ping.maybe_invoke()
                || impl.op_rd.maybe_invoke()
                || impl.op_wr.maybe_invoke();
            this->invoke(cont, ec);
        }
    }
};

//------------------------------------------------------------------------------

#if 0
template<
    class NextLayer,
    bool deflateSupported,
    class Handler>
void
async_auto_ping(
    stream<NextLayer, deflateSupported>& ws,
    Handler&& handler)
{
    using handler_type =
        typename std::decay<Handler>::type;

    using base_type =
        beast::stable_async_op_base<
            handler_type, beast::executor_type<
                stream<NextLayer, deflateSupported>>>;

    struct async_op : base_type, net::coroutine
    {
        boost::weak_ptr<impl_type> impl_;
        detail::frame_buffer& fb_;

    public:
        static constexpr int id = 4; // for soft_mutex

        async_op(
            Handler&& h,
            stream<NextLayer, deflateSupported>& ws)
            : base_type(std::move(h), ws.get_executor())
            , impl_(ws.impl_)
            , fb_(beast::allocate_stable<
                detail::frame_buffer>(*this))
        {
            // Serialize the ping or pong frame
            ping_data payload;
            ws.template write_ping<
                flat_static_buffer_base>(fb_, op, payload);
            (*this)({}, 0, false);
        }

        void operator()(
            error_code ec = {},
            std::size_t bytes_transferred = 0,
            bool cont = true)
        {
            boost::ignore_unused(bytes_transferred);
            auto sp = impl_.lock();
            if(! sp)
                return;

            auto& impl = *ws_.impl_;
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Acquire the write lock
                if(! impl.wr_block.try_lock(this))
                {
                    BOOST_ASIO_CORO_YIELD
                    impl.op_idle_ping.emplace(std::move(*this));
                    impl.wr_block.lock(this);
                    BOOST_ASIO_CORO_YIELD
                    net::post(std::move(*this));
                    BOOST_ASSERT(impl.wr_block.is_locked(this));
                }
                if(impl.check_stop_now(ec))
                    goto upcall;

                // Send ping frame
                BOOST_ASIO_CORO_YIELD
                net::async_write(impl.stream, fb_.data(),
                    beast::detail::bind_continuation(std::move(*this)));
                if(impl.check_stop_now(ec))
                    goto upcall;

            upcall:
                impl.wr_block.unlock(this);
                impl.op_close.maybe_invoke()
                    || impl.op_ping.maybe_invoke()
                    || impl.op_rd.maybe_invoke()
                    || impl.op_wr.maybe_invoke();
                if(! cont)
                {
                    BOOST_ASIO_CORO_YIELD
                    net::post(bind_front_handler(
                        std::move(*this), ec));
                }
                this->invoke(ec);
            }
        }
    };

    async_op op(ws, std::forward<Handler>(handler));
}
#endif

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
ping(ping_data const& payload)
{
    error_code ec;
    ping(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
ping(ping_data const& payload, error_code& ec)
{
    if(impl_->check_stop_now(ec))
        return;
    detail::frame_buffer fb;
    impl_->template write_ping<flat_static_buffer_base>(
        fb, detail::opcode::ping, payload);
    net::write(impl_->stream, fb.data(), ec);
    if(impl_->check_stop_now(ec))
        return;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
pong(ping_data const& payload)
{
    error_code ec;
    pong(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
pong(ping_data const& payload, error_code& ec)
{
    if(impl_->check_stop_now(ec))
        return;
    detail::frame_buffer fb;
    impl_->template write_ping<flat_static_buffer_base>(
        fb, detail::opcode::pong, payload);
    net::write(impl_->stream, fb.data(), ec);
    if(impl_->check_stop_now(ec))
        return;
}

template<class NextLayer, bool deflateSupported>
template<class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_ping(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code));
    ping_op<BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>(
            std::move(init.completion_handler), impl_,
                detail::opcode::ping, payload);
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_pong(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code));
    ping_op<BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>(
            std::move(init.completion_handler), impl_,
                detail::opcode::pong, payload);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
