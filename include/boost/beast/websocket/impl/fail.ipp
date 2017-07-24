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

// _Fail the WebSocket Connection_
//
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::fail_op
{
    Handler h_;
    stream<NextLayer>& ws_;
    int step_ = 0;
    bool dispatched_ = false;
    fail_how how_;
    token tok_;

public:
    fail_op(fail_op&&) = default;
    fail_op(fail_op const&) = default;

    // send close code, then teardown
    template<class DeducedHandler>
    fail_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        close_code code)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , how_(fail_how::code)
        , tok_(ws_.t_.unique())
    {
        ws_.rd_.fb.consume(ws_.rd_.fb.size());
        ws_.template write_close<
            flat_static_buffer_base>(
                ws_.rd_.fb, code);
    }

    // maybe send frame in fb, then teardown
    template<class DeducedHandler>
    fail_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        fail_how how)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , how_(how)
        , tok_(ws_.t_.unique())
    {
    }

    Handler&
    handler()
    {
        return h_;
    }

    void operator()(error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, fail_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, fail_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(fail_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->dispatched_ ||
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, fail_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f,
            std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::
fail_op<Handler>::
operator()(error_code ec, std::size_t)
{
    enum
    {
        do_start        = 0,
        do_resume       = 20,
        do_teardown     = 40
    };
    switch(step_)
    {
    case do_start:
        // Acquire write block
        if(ws_.wr_block_)
        {
            // suspend
            BOOST_ASSERT(ws_.wr_block_ != tok_);
            step_ = do_resume;
            ws_.rd_op_.save(std::move(*this));
            return;
        }
        ws_.wr_block_ = tok_;
        goto go_write;

    case do_resume:
        BOOST_ASSERT(! ws_.wr_block_);
        ws_.wr_block_ = tok_;
        step_ = do_resume + 1;
        // We were invoked from a foreign context, so post
        return ws_.get_io_service().post(std::move(*this));

    case do_resume + 1:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        dispatched_ = true;
    go_write:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        if(ws_.failed_)
        {
            ws_.wr_block_.reset();
            ec = boost::asio::error::operation_aborted;
            break;
        }
        if(how_ == fail_how::teardown)
            goto go_teardown;
        if(ws_.wr_close_)
            goto go_teardown;
        // send close frame
        step_ = do_teardown;
        ws_.wr_close_ = true;
        return boost::asio::async_write(
            ws_.stream_, ws_.rd_.fb.data(),
                std::move(*this));

    case do_teardown:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        dispatched_ = true;
        ws_.failed_ = !!ec;
        if(ws_.failed_)
        {
            ws_.wr_block_.reset();
            break;
        }
    go_teardown:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        step_ = do_teardown + 1;
        websocket_helpers::call_async_teardown(
            ws_.next_layer(), std::move(*this));
        return;

    case do_teardown + 1:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        dispatched_ = true;
        ws_.failed_ = true;
        ws_.wr_block_.reset();
        if(ec == boost::asio::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec.assign(0, ec.category());
        }
        if(! ec)
        {
            switch(how_)
            {
            default:
            case fail_how::code:
            case fail_how::teardown:    ec = error::failed; break;
            case fail_how::close:       ec = error::closed; break;
            }
        }
        break;
    }
    // upcall
    BOOST_ASSERT(ws_.wr_block_ != tok_);
    ws_.close_op_.maybe_invoke() ||
        ws_.ping_op_.maybe_invoke() ||
        ws_.wr_op_.maybe_invoke();
    if(! dispatched_)
        ws_.stream_.get_io_service().post(
            bind_handler(std::move(h_), ec));
    else
        h_(ec);
}

} // websocket
} // beast
} // boost

#endif
