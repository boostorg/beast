//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_FAIL_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_FAIL_IPP

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {

/*
    This composed operation optionally sends a close frame,
    then performs the teardown operation.
*/
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::fail_op
    : public boost::asio::coroutine
{
    struct state
    {
        stream<NextLayer>& ws;
        detail::frame_streambuf fb;
        std::uint16_t code;
        error_code ev;
        token tok;

        state(
            Handler&,
            stream<NextLayer>& ws_,
            std::uint16_t code_,
            error_code ev_)
            : ws(ws_)
            , code(code_)
            , ev(ev_)
            , tok(ws.t_.unique())
        {
        }
    };

    handler_ptr<state, Handler> d_;

public:
    fail_op(fail_op&&) = default;
    fail_op(fail_op const&) = default;

    template<class DeducedHandler>
    fail_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        std::uint16_t code,
        error_code ev)
        : d_(std::forward<DeducedHandler>(h),
            ws, code, ev)
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, fail_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, fail_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(fail_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, fail_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f,
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::
fail_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
        if(! d.ws.wr_block_)
        {
            // Acquire the write block
            d.ws.wr_block_ = d.tok;

            // Make sure the stream is open
            if(d.ws.failed_)
            {
                BOOST_ASIO_CORO_YIELD
                d.ws.get_io_service().post(
                    bind_handler(std::move(*this),
                        boost::asio::error::operation_aborted));
                goto upcall;
            }
        }
        else
        {
            // Suspend
            BOOST_ASSERT(d.ws.wr_block_ != d.tok);
            BOOST_ASIO_CORO_YIELD
            d.ws.rd_op_.emplace(std::move(*this)); // VFALCO emplace to rd_op_

            // Acquire the write block
            BOOST_ASSERT(! d.ws.wr_block_);
            d.ws.wr_block_ = d.tok;

            // Resume
            BOOST_ASIO_CORO_YIELD
            d.ws.get_io_service().post(std::move(*this));
            BOOST_ASSERT(d.ws.wr_block_ == d.tok);

            // Make sure the stream is open
            if(d.ws.failed_)
            {
                ec = boost::asio::error::operation_aborted;
                goto upcall;
            }
        }
        if(d.code != close_code::none && ! d.ws.wr_close_)
        {
            // Serialize close frame
            d.ws.template write_close<
                flat_static_buffer_base>(
                    d.fb, d.code);
            // Send close frame
            d.ws.wr_close_ = true;
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(
                d.ws.stream_, d.fb.data(),
                    std::move(*this));
            BOOST_ASSERT(d.ws.wr_block_ == d.tok);
            d.ws.failed_ = !!ec;
            if(d.ws.failed_)
                goto upcall;
        }
        // Teardown
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        using beast::websocket::async_teardown;
        BOOST_ASIO_CORO_YIELD
        async_teardown(d.ws.role_,
            d.ws.stream_, std::move(*this));
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        if(ec == boost::asio::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec.assign(0, ec.category());
        }
        if(! ec)
            ec = d.ev;
        d.ws.failed_ = true;
    upcall:
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        d.ws.wr_block_.reset();
        d.ws.close_op_.maybe_invoke() ||
            d.ws.ping_op_.maybe_invoke() ||
            d.ws.wr_op_.maybe_invoke();
        d_.invoke(ec, 0);
    }
}

//------------------------------------------------------------------------------

/*  _Fail the WebSocket Connection_
*/
template<class NextLayer>
void
stream<NextLayer>::
do_fail(
    std::uint16_t code,         // if set, send a close frame first
    error_code ev,              // error code to use upon success
    error_code& ec)             // set to the error, else set to ev
{
    BOOST_ASSERT(ev);
    if(code != close_code::none && ! wr_close_)
    {
        wr_close_ = true;
        detail::frame_streambuf fb;
        write_close<flat_static_buffer_base>(fb, code);
        boost::asio::write(stream_, fb.data(), ec);
        failed_ = !!ec;
        if(failed_)
            return;
    }
    using beast::websocket::teardown;
    teardown(role_, stream_, ec);
    if(ec == boost::asio::error::eof)
    {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec.assign(0, ec.category());
    }
    failed_ = !!ec;
    if(failed_)
        return;
    ec = ev;
    failed_ = true;
}

/*  _Fail the WebSocket Connection_
*/
template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::
do_async_fail(
    std::uint16_t code,         // if set, send a close frame first
    error_code ev,              // error code to use upon success
    Handler&& handler)
{
    fail_op<typename std::decay<Handler>::type>{
        std::forward<Handler>(handler),
        *this,
        code,
        ev}();
}

} // websocket
} // beast
} // boost

#endif
