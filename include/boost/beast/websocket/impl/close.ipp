//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_IPP

#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

//------------------------------------------------------------------------------

// send the close message and wait for the response
//
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::close_op
{
    struct data : op
    {
        stream<NextLayer>& ws;
        close_reason cr;
        detail::frame_streambuf fb;
        int state = 0;
        token tok;

        data(Handler&, stream<NextLayer>& ws_,
                close_reason const& cr_)
            : ws(ws_)
            , cr(cr_)
            , tok(ws.t_.unique())
        {
            ws.template write_close<
                flat_static_buffer_base>(fb, cr);
        }
    };

    handler_ptr<data, Handler> d_;

public:
    close_op(close_op&&) = default;
    close_op(close_op const&) = default;

    template<class DeducedHandler, class... Args>
    close_op(DeducedHandler&& h,
            stream<NextLayer>& ws, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            ws, std::forward<Args>(args)...)
    {
    }

    void operator()()
    {
        (*this)({});
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, close_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, close_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(close_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, close_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::close_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    if(ec)
    {
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        d.ws.failed_ = true;
        goto upcall;
    }
    switch(d.state)
    {
    case 0:
        if(d.ws.wr_block_)
        {
            // suspend
            d.state = 1;
            d.ws.close_op_.emplace(std::move(*this));
            return;
        }
        d.ws.wr_block_ = d.tok;
        if(d.ws.failed_ || d.ws.wr_close_)
        {
            // call handler
            d.ws.get_io_service().post(
                bind_handler(std::move(*this),
                    boost::asio::error::operation_aborted));
            return;
        }

    do_write:
        // send close frame
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        d.state = 3;
        d.ws.wr_close_ = true;
        boost::asio::async_write(d.ws.stream_,
            d.fb.data(), std::move(*this));
        return;

    case 1:
        BOOST_ASSERT(! d.ws.wr_block_);
        d.ws.wr_block_ = d.tok;
        d.state = 2;
        // The current context is safe but might not be
        // the same as the one for this operation (since
        // we are being called from a write operation).
        // Call post to make sure we are invoked the same
        // way as the final handler for this operation.
        d.ws.get_io_service().post(
            bind_handler(std::move(*this), ec));
        return;

    case 2:
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        if(d.ws.failed_ || d.ws.wr_close_)
        {
            // call handler
            ec = boost::asio::error::operation_aborted;
            goto upcall;
        }
        goto do_write;

    case 3:
        break;
    }
upcall:
    BOOST_ASSERT(d.ws.wr_block_ == d.tok);
    d.ws.wr_block_.reset();
    d.ws.rd_op_.maybe_invoke() ||
        d.ws.ping_op_.maybe_invoke() ||
        d.ws.wr_op_.maybe_invoke();
    d_.invoke(ec);
}

//------------------------------------------------------------------------------

template<class NextLayer>
void
stream<NextLayer>::
close(close_reason const& cr)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    close(cr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
close(close_reason const& cr, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    using beast::detail::clamp;
    // If rd_close_ is set then we already sent a close
    BOOST_ASSERT(! rd_close_);
    if(wr_close_)
    {
        // Can't call close twice, abort operation
        BOOST_ASSERT(! wr_close_);
        ec = boost::asio::error::operation_aborted;
        return;
    }
    wr_close_ = true;
    {
        detail::frame_streambuf fb;
        write_close<flat_static_buffer_base>(fb, cr);
        boost::asio::write(stream_, fb.data(), ec);
    }
    failed_ = !!ec;
    if(failed_)
        return;
    // Drain the connection
    close_code code{};
    if(rd_.remain > 0)
        goto read_payload;
    for(;;)
    {
        // Read frame header
        while(! parse_fh(rd_.fh, rd_.buf, code))
        {
            if(code != close_code::none)
                return do_fail(close_code::none,
                    error::failed, ec);
            auto const bytes_transferred =
                stream_.read_some(
                    rd_.buf.prepare(read_size(rd_.buf,
                        rd_.buf.max_size())), ec);
            failed_ = !!ec;
            if(failed_)
                return;
            rd_.buf.commit(bytes_transferred);
        }
        if(detail::is_control(rd_.fh.op))
        {
            // Process control frame
            if(rd_.fh.op == detail::opcode::close)
            {
                BOOST_ASSERT(! rd_close_);
                rd_close_ = true;
                auto const mb = buffer_prefix(
                    clamp(rd_.fh.len),
                    rd_.buf.mutable_data());
                if(rd_.fh.len > 0 && rd_.fh.mask)
                    detail::mask_inplace(mb, rd_.key);
                detail::read_close(cr_, mb, code);
                if(code != close_code::none)
                    // Protocol error
                    return do_fail(close_code::none,
                        error::failed, ec);
                rd_.buf.consume(clamp(rd_.fh.len));
                break;
            }
            rd_.buf.consume(clamp(rd_.fh.len));
        }
        else
        {
        read_payload:
            while(rd_.buf.size() < rd_.remain)
            {
                rd_.remain -= rd_.buf.size();
                rd_.buf.consume(rd_.buf.size());
                auto const bytes_transferred =
                    stream_.read_some(
                        rd_.buf.prepare(read_size(rd_.buf,
                            rd_.buf.max_size())), ec);
                failed_ = !!ec;
                if(failed_)
                    return;
                rd_.buf.commit(bytes_transferred);
            }
            BOOST_ASSERT(rd_.buf.size() >= rd_.remain);
            rd_.buf.consume(clamp(rd_.remain));
            rd_.remain = 0;
        }
    }
    // _Close the WebSocket Connection_
    do_fail(close_code::none, error::closed, ec);
    if(ec == error::closed)
        ec.assign(0, ec.category());
}

template<class NextLayer>
template<class CloseHandler>
async_return_type<
    CloseHandler, void(error_code)>
stream<NextLayer>::
async_close(close_reason const& cr, CloseHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    async_completion<CloseHandler,
        void(error_code)> init{handler};
    close_op<handler_type<
        CloseHandler, void(error_code)>>{
            init.completion_handler, *this, cr}({});
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
