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
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
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
    struct state
    {
        stream<NextLayer, deflateSupported>& ws;
        detail::frame_buffer fb;

        state(
            stream<NextLayer, deflateSupported>& ws_,
            detail::opcode op,
            ping_data const& payload)
            : ws(ws_)
        {
            // Serialize the control frame
            ws.template write_ping<
                flat_static_buffer_base>(
                    fb, op, payload);
        }
    };

    state& d_;

public:
    static constexpr int id = 3; // for soft_mutex

    template<class Handler_>
    ping_op(
        Handler_&& h,
        stream<NextLayer, deflateSupported>& ws,
        detail::opcode op,
        ping_data const& payload)
        : stable_async_op_base<
            Handler, beast::executor_type<stream>>(
                std::forward<Handler_>(h), ws.get_executor())
        , d_(beast::allocate_stable<state>(
            *this, ws, op, payload))
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        boost::ignore_unused(bytes_transferred);

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Maybe suspend
            if(d_.ws.impl_->wr_block.try_lock(this))
            {
                // Make sure the stream is open
                if(! d_.ws.impl_->check_open(ec))
                {
                    BOOST_ASIO_CORO_YIELD
                    net::post(
                        d_.ws.get_executor(),
                        beast::bind_front_handler(std::move(*this), ec));
                    goto upcall;
                }
            }
            else
            {
                // Suspend
                BOOST_ASIO_CORO_YIELD
                d_.ws.impl_->paused_ping.emplace(std::move(*this));

                // Acquire the write block
                d_.ws.impl_->wr_block.lock(this);

                // Resume
                BOOST_ASIO_CORO_YIELD
                net::post(
                    d_.ws.get_executor(), std::move(*this));
                BOOST_ASSERT(d_.ws.impl_->wr_block.is_locked(this));

                // Make sure the stream is open
                if(! d_.ws.impl_->check_open(ec))
                    goto upcall;
            }

            // Send ping frame
            BOOST_ASIO_CORO_YIELD
            net::async_write(d_.ws.impl_->stream,
                d_.fb.data(), std::move(*this));
            if(! d_.ws.impl_->check_ok(ec))
                goto upcall;

        upcall:
            d_.ws.impl_->wr_block.unlock(this);
            d_.ws.impl_->paused_close.maybe_invoke() ||
                d_.ws.impl_->paused_rd.maybe_invoke() ||
                d_.ws.impl_->paused_wr.maybe_invoke();
            this->invoke(ec);
        }
    }
};

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
    // Make sure the stream is open
    if(! impl_->check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::ping, payload);
    net::write(impl_->stream, fb.data(), ec);
    if(! impl_->check_ok(ec))
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
    // Make sure the stream is open
    if(! impl_->check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::pong, payload);
    net::write(impl_->stream, fb.data(), ec);
    if(! impl_->check_ok(ec))
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
        WriteHandler, void(error_code))>{
            std::move(init.completion_handler), *this,
                detail::opcode::ping, payload}();
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
        WriteHandler, void(error_code))>{
            std::move(init.completion_handler), *this,
                detail::opcode::pong, payload}();
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
