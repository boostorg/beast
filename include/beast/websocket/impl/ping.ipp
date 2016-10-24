//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_PING_IPP
#define BEAST_WEBSOCKET_IMPL_PING_IPP

#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_alloc.hpp>
#include <beast/core/stream_concepts.hpp>
#include <beast/websocket/detail/frame.hpp>
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
    using alloc_type =
        handler_alloc<char, Handler>;

    struct data : op
    {
        stream<NextLayer>& ws;
        Handler h;
        detail::frame_streambuf fb;
        bool cont;
        int state = 0;

        template<class DeducedHandler>
        data(DeducedHandler&& h_, stream<NextLayer>& ws_,
                ping_data const& payload)
            : ws(ws_)
            , h(std::forward<DeducedHandler>(h_))
            , cont(boost_asio_handler_cont_helpers::
                is_continuation(h))
        {
            using boost::asio::buffer;
            using boost::asio::buffer_copy;
            ws.template write_ping<static_streambuf>(
                fb, opcode::ping, payload);
        }
    };

    std::shared_ptr<data> d_;

public:
    ping_op(ping_op&&) = default;
    ping_op(ping_op const&) = default;

    template<class DeducedHandler, class... Args>
    ping_op(DeducedHandler&& h,
            stream<NextLayer>& ws, Args&&... args)
        : d_(std::make_shared<data>(
            std::forward<DeducedHandler>(h), ws,
                std::forward<Args>(args)...))
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
        return boost_asio_handler_alloc_helpers::
            allocate(size, op->d_->h);
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, ping_op* op)
    {
        return boost_asio_handler_alloc_helpers::
            deallocate(p, size, op->d_->h);
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
        return boost_asio_handler_invoke_helpers::
            invoke(f, op->d_->h);
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
                d.ws.wr_op_.template emplace<
                    ping_op>(std::move(*this));
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
            // fall through

        case 1:
            // send ping frame
            d.state = 99;
            BOOST_ASSERT(! d.ws.wr_block_);
            d.ws.wr_block_ = &d;
            boost::asio::async_write(d.ws.stream_,
                d.fb.data(), std::move(*this));
            return;

        case 2:
            d.state = 3;
            d.ws.get_io_service().post(
                bind_handler(std::move(*this), ec));
            return;

        case 3:
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
    d.ws.rd_op_.maybe_invoke();
    d.h(ec);
}

template<class NextLayer>
template<class PingHandler>
typename async_completion<
    PingHandler, void(error_code)>::result_type
stream<NextLayer>::
async_ping(ping_data const& payload, PingHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    beast::async_completion<
        PingHandler, void(error_code)
            > completion(handler);
    ping_op<decltype(completion.handler)>{
        completion.handler, *this, payload};
    return completion.result.get();
}

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload)
{
    error_code ec;
    ping(payload, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload, error_code& ec)
{
    detail::frame_streambuf db;
    write_ping<static_streambuf>(
        db, opcode::ping, payload);
    boost::asio::write(stream_, db.data(), ec);
}

//------------------------------------------------------------------------------

} // websocket
} // beast

#endif
