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
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

//------------------------------------------------------------------------------

/*  Close the WebSocket Connection

    This composed operation sends the close frame if it hasn't already
    been sent, then reads and discards frames until receiving a close
    frame. Finally it invokes the teardown operation to shut down the
    underlying connection.
*/
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::close_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    token tok_;

public:
    close_op(close_op&&) = default;
    close_op(close_op const&) = default;

    template<class DeducedHandler>
    close_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        close_reason const& cr)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
    {
        // Serialize the close frame
        ws_.wr_.fb.reset();
        ws_.template write_close<
            flat_static_buffer_base>(
                ws_.wr_.fb, cr);
    }

    Handler&
    handler()
    {
        return h_;
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, close_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, close_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(close_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, close_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::close_op<Handler>::
operator()(error_code ec, std::size_t)
{
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
        if(! ws_.wr_block_)
        {
            // Acquire the write block
            ws_.wr_block_ = tok_;

            // Make sure the stream is open
            if(ws_.failed_)
            {
                BOOST_ASIO_CORO_YIELD
                ws_.get_io_service().post(
                    bind_handler(std::move(*this),
                        boost::asio::error::operation_aborted));
                goto upcall;
            }
        }
        else
        {
            // Suspend
            BOOST_ASSERT(ws_.wr_block_ != tok_);
            BOOST_ASIO_CORO_YIELD
            ws_.close_op_.save(std::move(*this));

            // Acquire the write block
            BOOST_ASSERT(! ws_.wr_block_);
            ws_.wr_block_ = tok_;

            // Resume
            BOOST_ASIO_CORO_YIELD
            ws_.get_io_service().post(std::move(*this));
            BOOST_ASSERT(ws_.wr_block_ == tok_);

            // Make sure the stream is open
            if(ws_.failed_)
            {
                ec = boost::asio::error::operation_aborted;
                goto upcall;
            }
        }

        // Send close frame
        BOOST_ASIO_CORO_YIELD
        boost::asio::async_write(ws_.stream_,
            ws_.wr_.fb.data(), std::move(*this));
        if(ec)
            ws_.failed_ = true;

    upcall:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        ws_.wr_block_.reset();
        ws_.rd_op_.maybe_invoke() ||
            ws_.ping_op_.maybe_invoke() ||
            ws_.wr_op_.maybe_invoke();
        h_(ec);
    }
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
    // Make sure the stream is open
    if(failed_)
    {
        ec = boost::asio::error::operation_aborted;
        return;
    }
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
                    rd_.buf.data());
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
