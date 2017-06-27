//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_PING_IPP
#define BEAST_WEBSOCKET_IMPL_PING_IPP

#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/core/detail/config.hpp>
#include <beast/websocket/detail/frame.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace beast {
namespace websocket {

//------------------------------------------------------------------------------

// write a ping frame
//
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::ping_op
{
    struct data : op
    {
        bool cont;
        stream<NextLayer>& ws;
        detail::frame_streambuf fb;
        int state = 0;

        data(Handler& handler, stream<NextLayer>& ws_,
                detail::opcode op_, ping_data const& payload)
            : ws(ws_)
        {
            using boost::asio::asio_handler_is_continuation;
            cont = asio_handler_is_continuation(std::addressof(handler));
            using boost::asio::buffer;
            using boost::asio::buffer_copy;
            ws.template write_ping<
                static_buffer>(fb, op_, payload);
        }
    };

    handler_ptr<data, Handler> d_;

public:
    ping_op(ping_op&&) = default;
    ping_op(ping_op const&) = default;

    template<class DeducedHandler, class... Args>
    ping_op(DeducedHandler&& h,
            stream<NextLayer>& ws, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            ws, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, false);
    }

    void operator()()
    {
        (*this)(error_code{});
    }

    void operator()(error_code ec, std::size_t);

    void operator()(error_code ec, bool again = true);

    friend
    void* asio_handler_allocate(
        std::size_t size, ping_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, ping_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(ping_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, ping_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::ping_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    if(ec)
        d.ws.failed_ = true;
    (*this)(ec);
}

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::
ping_op<Handler>::
operator()(error_code ec, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(ec)
        goto upcall;
    for(;;)
    {
        switch(d.state)
        {
        case 0:
            if(d.ws.wr_block_)
            {
                // suspend
                d.state = 2;
                d.ws.ping_op_.emplace(std::move(*this));
                return;
            }
            if(d.ws.failed_ || d.ws.wr_close_)
            {
                // call handler
                d.state = 99;
                d.ws.get_io_service().post(
                    bind_handler(std::move(*this),
                        boost::asio::error::operation_aborted));
                return;
            }
            d.ws.wr_block_ = &d;
            BEAST_FALLTHROUGH;

        case 1:
            // send ping frame
            BOOST_ASSERT(d.ws.wr_block_ == &d);
            d.state = 99;
            boost::asio::async_write(d.ws.stream_,
                d.fb.data(), std::move(*this));
            return;

        case 2:
            BOOST_ASSERT(! d.ws.wr_block_);
            d.ws.wr_block_ = &d;
            d.state = 3;
            // The current context is safe but might not be
            // the same as the one for this operation (since
            // we are being called from a write operation).
            // Call post to make sure we are invoked the same
            // way as the final handler for this operation.
            d.ws.get_io_service().post(
                bind_handler(std::move(*this), ec));
            return;

        case 3:
            BOOST_ASSERT(d.ws.wr_block_ == &d);
            if(d.ws.failed_ || d.ws.wr_close_)
            {
                // call handler
                ec = boost::asio::error::operation_aborted;
                goto upcall;
            }
            d.state = 1;
            break;

        case 99:
            goto upcall;
        }
    }
upcall:
    if(d.ws.wr_block_ == &d)
        d.ws.wr_block_ = nullptr;
    d.ws.rd_op_.maybe_invoke() ||
        d.ws.wr_op_.maybe_invoke();
    d_.invoke(ec);
}

template<class NextLayer>
template<class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
stream<NextLayer>::
async_ping(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    ping_op<handler_type<
        WriteHandler, void(error_code)>>{
            init.completion_handler, *this,
                detail::opcode::ping, payload};
    return init.result.get();
}

template<class NextLayer>
template<class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
stream<NextLayer>::
async_pong(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    ping_op<handler_type<
        WriteHandler, void(error_code)>>{
            init.completion_handler, *this,
                detail::opcode::pong, payload};
    return init.result.get();
}

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload)
{
    error_code ec;
    ping(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload, error_code& ec)
{
    detail::frame_streambuf db;
    write_ping<static_buffer>(
        db, detail::opcode::ping, payload);
    boost::asio::write(stream_, db.data(), ec);
}

template<class NextLayer>
void
stream<NextLayer>::
pong(ping_data const& payload)
{
    error_code ec;
    pong(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
pong(ping_data const& payload, error_code& ec)
{
    detail::frame_streambuf db;
    write_ping<static_buffer>(
        db, detail::opcode::pong, payload);
    boost::asio::write(stream_, db.data(), ec);
}

//------------------------------------------------------------------------------

} // websocket
} // beast

#endif
