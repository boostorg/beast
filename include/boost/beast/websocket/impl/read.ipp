//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_READ_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_READ_IPP

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_prefix.hpp>
#include <boost/beast/core/consuming_buffers.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <limits>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*  Read some message frame data.

    Also reads and handles control frames.
*/
template<class NextLayer>
template<
    class MutableBufferSequence,
    class Handler>
class stream<NextLayer>::read_some_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    consuming_buffers<MutableBufferSequence> cb_;
    std::size_t bytes_written_ = 0;
    error_code ev_;
    token tok_;
    bool dispatched_ = false;
    bool did_read_ = false;

public:
    read_some_op(read_some_op&&) = default;
    read_some_op(read_some_op const&) = default;

    template<class DeducedHandler>
    read_some_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        MutableBufferSequence const& bs)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , cb_(bs)
        , tok_(ws_.t_.unique())
    {
    }

    Handler&
    handler()
    {
        return h_;
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_some_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_some_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(p, size,
            std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(read_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->dispatched_ ||
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class MutableBufferSequence, class Handler>
void
stream<NextLayer>::
read_some_op<MutableBufferSequence, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    close_code code{};
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
        if(! ws_.rd_block_)
        {
            // Acquire the read block
            ws_.rd_block_ = tok_;

            // Make sure the stream is open
            if(! ws_.open_)
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
            BOOST_ASSERT(ws_.rd_block_ != tok_);
            BOOST_ASIO_CORO_YIELD
            ws_.r_rd_op_.save(std::move(*this));

            // Acquire the read block
            BOOST_ASSERT(! ws_.rd_block_);
            ws_.rd_block_ = tok_;

            // Resume
            BOOST_ASIO_CORO_YIELD
            ws_.get_io_service().post(std::move(*this));
            BOOST_ASSERT(ws_.rd_block_ == tok_);
            dispatched_ = true;

            // Handle the stream closing while suspended
            if(! ws_.open_)
            {
                ec = boost::asio::error::operation_aborted;
                goto upcall;
            }
        }
    loop:
        // See if we need to read a frame header. This
        // condition is structured to give the decompressor
        // a chance to emit the final empty deflate block
        //
        if(ws_.rd_.remain == 0 &&
            (! ws_.rd_.fh.fin || ws_.rd_.done))
        {
            // Read frame header
            while(! ws_.parse_fh(
                ws_.rd_.fh, ws_.rd_.buf, code))
            {
                if(code != close_code::none)
                {
                    // _Fail the WebSocket Connection_
                    ec = error::failed;
                    goto close;
                }
                BOOST_ASIO_CORO_YIELD
                ws_.stream_.async_read_some(
                    ws_.rd_.buf.prepare(read_size(
                        ws_.rd_.buf, ws_.rd_.buf.max_size())),
                            std::move(*this));
                dispatched_ = true;
                ws_.open_ = ! ec;
                if(! ws_.open_)
                    goto upcall;
                ws_.rd_.buf.commit(bytes_transferred);
            }
            // Immediately apply the mask to the portion
            // of the buffer holding payload data.
            if(ws_.rd_.fh.len > 0 && ws_.rd_.fh.mask)
                detail::mask_inplace(buffer_prefix(
                    clamp(ws_.rd_.fh.len),
                        ws_.rd_.buf.data()),
                            ws_.rd_.key);
            if(detail::is_control(ws_.rd_.fh.op))
            {
                // Clear this otherwise the next
                // frame will be considered final.
                ws_.rd_.fh.fin = false;

                // Handle ping frame
                if(ws_.rd_.fh.op == detail::opcode::ping)
                {
                    {
                        auto const b = buffer_prefix(
                            clamp(ws_.rd_.fh.len),
                                ws_.rd_.buf.data());
                        auto const len = buffer_size(b);
                        BOOST_ASSERT(len == ws_.rd_.fh.len);
                        ping_data payload;
                        detail::read_ping(payload, b);
                        ws_.rd_.buf.consume(len);
                        // Ignore ping when closing
                        if(ws_.wr_close_)
                            goto loop;
                        if(ws_.ctrl_cb_)
                            ws_.ctrl_cb_(frame_type::ping, payload);
                        ws_.rd_.fb.reset();
                        ws_.template write_ping<
                            flat_static_buffer_base>(ws_.rd_.fb,
                                detail::opcode::pong, payload);
                    }
                    // Maybe suspend
                    if(! ws_.wr_block_)
                    {
                        // Acquire the write block
                        ws_.wr_block_ = tok_;
                    }
                    else
                    {
                        // Suspend
                        BOOST_ASSERT(ws_.wr_block_ != tok_);
                        BOOST_ASIO_CORO_YIELD
                        ws_.rd_op_.save(std::move(*this));

                        // Acquire the write block
                        BOOST_ASSERT(! ws_.wr_block_);
                        ws_.wr_block_ = tok_;

                        // Resume
                        BOOST_ASIO_CORO_YIELD
                        ws_.get_io_service().post(std::move(*this));
                        BOOST_ASSERT(ws_.wr_block_ == tok_);
                        dispatched_ = true;

                        // Make sure the stream is open
                        if(! ws_.open_)
                        {
                            ws_.wr_block_.reset();
                            ec = boost::asio::error::operation_aborted;
                            goto upcall;
                        }

                        // Ignore ping when closing
                        if(ws_.wr_close_)
                        {
                            ws_.wr_block_.reset();
                            goto loop;
                        }
                    }

                    // Send pong
                    BOOST_ASSERT(ws_.wr_block_ == tok_);
                    BOOST_ASIO_CORO_YIELD
                    boost::asio::async_write(ws_.stream_,
                        ws_.rd_.fb.data(), std::move(*this));
                    BOOST_ASSERT(ws_.wr_block_ == tok_);
                    dispatched_ = true;
                    ws_.wr_block_.reset();
                    ws_.open_ = ! ec;
                    if(! ws_.open_)
                        goto upcall;
                    goto loop;
                }
                // Handle pong frame
                if(ws_.rd_.fh.op == detail::opcode::pong)
                {
                    auto const cb = buffer_prefix(clamp(
                        ws_.rd_.fh.len), ws_.rd_.buf.data());
                    auto const len = buffer_size(cb);
                    BOOST_ASSERT(len == ws_.rd_.fh.len);
                    code = close_code::none;
                    ping_data payload;
                    detail::read_ping(payload, cb);
                    ws_.rd_.buf.consume(len);
                    // Ignore pong when closing
                    if(! ws_.wr_close_ && ws_.ctrl_cb_)
                        ws_.ctrl_cb_(frame_type::pong, payload);
                    goto loop;
                }
                // Handle close frame
                BOOST_ASSERT(ws_.rd_.fh.op == detail::opcode::close);
                {
                    auto const cb = buffer_prefix(clamp(
                        ws_.rd_.fh.len), ws_.rd_.buf.data());
                    auto const len = buffer_size(cb);
                    BOOST_ASSERT(len == ws_.rd_.fh.len);
                    BOOST_ASSERT(! ws_.rd_close_);
                    ws_.rd_close_ = true;
                    close_reason cr;
                    detail::read_close(cr, cb, code);
                    if(code != close_code::none)
                    {
                        // _Fail the WebSocket Connection_
                        ec = error::failed;
                        goto close;
                    }
                    ws_.cr_ = cr;
                    ws_.rd_.buf.consume(len);
                    if(ws_.ctrl_cb_)
                        ws_.ctrl_cb_(frame_type::close,
                            ws_.cr_.reason);
                    if(! ws_.wr_close_)
                    {
                        // _Start the WebSocket Closing Handshake_
                        code = cr.code == close_code::none ?
                            close_code::normal :
                            static_cast<close_code>(cr.code);
                        ec = error::closed;
                        goto close;
                    }
                    // _Close the WebSocket Connection_
                    code = close_code::none;
                    ec = error::closed;
                    goto close;
                }
            }
            if(ws_.rd_.fh.len == 0 && ! ws_.rd_.fh.fin)
            {
                // Empty non-final frame
                goto loop;
            }
            ws_.rd_.done = false;
        }
        if(! ws_.pmd_ || ! ws_.pmd_->rd_set)
        {
            if(ws_.rd_.remain > 0)
            {
                if(ws_.rd_.buf.size() == 0 && ws_.rd_.buf.max_size() >
                    (std::min)(clamp(ws_.rd_.remain),
                        buffer_size(cb_)))
                {
                    // Fill the read buffer first, otherwise we
                    // get fewer bytes at the cost of one I/O.
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(
                        ws_.rd_.buf.prepare(read_size(
                            ws_.rd_.buf, ws_.rd_.buf.max_size())),
                                std::move(*this));
                    dispatched_ = true;
                    ws_.open_ = ! ec;
                    if(! ws_.open_)
                        goto upcall;
                    ws_.rd_.buf.commit(bytes_transferred);
                    if(ws_.rd_.fh.mask)
                        detail::mask_inplace(buffer_prefix(clamp(
                            ws_.rd_.remain), ws_.rd_.buf.data()),
                                ws_.rd_.key);
                }
                if(ws_.rd_.buf.size() > 0)
                {
                    // Copy from the read buffer.
                    // The mask was already applied.
                    bytes_transferred = buffer_copy(cb_,
                        ws_.rd_.buf.data(), clamp(ws_.rd_.remain));
                    auto const mb = buffer_prefix(
                        bytes_transferred, cb_);
                    ws_.rd_.remain -= bytes_transferred;
                    if(ws_.rd_.op == detail::opcode::text)
                    {
                        if(! ws_.rd_.utf8.write(mb) ||
                            (ws_.rd_.remain == 0 && ws_.rd_.fh.fin &&
                                ! ws_.rd_.utf8.finish()))
                        {
                            // _Fail the WebSocket Connection_
                            code = close_code::bad_payload;
                            ec = error::failed;
                            goto close;
                        }
                    }
                    bytes_written_ += bytes_transferred;
                    ws_.rd_.size += bytes_transferred;
                    ws_.rd_.buf.consume(bytes_transferred);
                }
                else
                {
                    // Read into caller's buffer
                    BOOST_ASSERT(ws_.rd_.remain > 0);
                    BOOST_ASSERT(buffer_size(cb_) > 0);
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(buffer_prefix(
                        clamp(ws_.rd_.remain), cb_), std::move(*this));
                    dispatched_ = true;
                    ws_.open_ = ! ec;
                    if(! ws_.open_)
                        goto upcall;
                    BOOST_ASSERT(bytes_transferred > 0);
                    auto const mb = buffer_prefix(
                        bytes_transferred, cb_);
                    ws_.rd_.remain -= bytes_transferred;
                    if(ws_.rd_.fh.mask)
                        detail::mask_inplace(mb, ws_.rd_.key);
                    if(ws_.rd_.op == detail::opcode::text)
                    {
                        if(! ws_.rd_.utf8.write(mb) ||
                            (ws_.rd_.remain == 0 && ws_.rd_.fh.fin &&
                                ! ws_.rd_.utf8.finish()))
                        {
                            // _Fail the WebSocket Connection_
                            code = close_code::bad_payload;
                            ec = error::failed;
                            goto close;
                        }
                    }
                    bytes_written_ += bytes_transferred;
                    ws_.rd_.size += bytes_transferred;
                }
            }
            ws_.rd_.done = ws_.rd_.remain == 0 && ws_.rd_.fh.fin;
            goto upcall;
        }
        else
        {
            // Read compressed message frame payload:
            // inflate even if rd_.fh.len == 0, otherwise we
            // never emit the end-of-stream deflate block.
            while(buffer_size(cb_) > 0)
            {
                if( ws_.rd_.remain > 0 &&
                    ws_.rd_.buf.size() == 0 &&
                    ! did_read_)
                {
                    // read new
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(
                        ws_.rd_.buf.prepare(read_size(
                            ws_.rd_.buf, ws_.rd_.buf.max_size())),
                                std::move(*this));
                    ws_.open_ = ! ec;
                    if(! ws_.open_)
                        goto upcall;
                    BOOST_ASSERT(bytes_transferred > 0);
                    ws_.rd_.buf.commit(bytes_transferred);
                    if(ws_.rd_.fh.mask)
                        detail::mask_inplace(
                            buffer_prefix(clamp(ws_.rd_.remain),
                                ws_.rd_.buf.data()), ws_.rd_.key);
                    did_read_ = true;
                }
                zlib::z_params zs;
                {
                    auto const out = buffer_front(cb_);
                    zs.next_out = buffer_cast<void*>(out);
                    zs.avail_out = buffer_size(out);
                    BOOST_ASSERT(zs.avail_out > 0);
                }
                if(ws_.rd_.remain > 0)
                {
                    if(ws_.rd_.buf.size() > 0)
                    {
                        // use what's there
                        auto const in = buffer_prefix(
                            clamp(ws_.rd_.remain), buffer_front(
                                ws_.rd_.buf.data()));
                        zs.avail_in = buffer_size(in);
                        zs.next_in = buffer_cast<void const*>(in);
                    }
                    else
                    {
                        break;
                    }
                }
                else if(ws_.rd_.fh.fin)
                {
                    // append the empty block codes
                    static std::uint8_t constexpr
                        empty_block[4] = {
                            0x00, 0x00, 0xff, 0xff };
                    zs.next_in = empty_block;
                    zs.avail_in = sizeof(empty_block);
                    ws_.pmd_->zi.write(zs, zlib::Flush::sync, ec);
                    BOOST_ASSERT(! ec);
                    ws_.open_ = ! ec;
                    if(! ws_.open_)
                        break;
                    // VFALCO See:
                    // https://github.com/madler/zlib/issues/280
                    BOOST_ASSERT(zs.total_out == 0);
                    cb_.consume(zs.total_out);
                    ws_.rd_.size += zs.total_out;
                    bytes_written_ += zs.total_out;
                    if(
                        (ws_.role_ == role_type::client &&
                            ws_.pmd_config_.server_no_context_takeover) ||
                        (ws_.role_ == role_type::server &&
                            ws_.pmd_config_.client_no_context_takeover))
                        ws_.pmd_->zi.reset();
                    ws_.rd_.done = true;
                    break;
                }
                else
                {
                    break;
                }
                ws_.pmd_->zi.write(zs, zlib::Flush::sync, ec);
                BOOST_ASSERT(ec != zlib::error::end_of_stream);
                ws_.open_ = ! ec;
                if(! ws_.open_)
                    break;
                if(ws_.rd_msg_max_ && beast::detail::sum_exceeds(
                    ws_.rd_.size, zs.total_out, ws_.rd_msg_max_))
                {
                    // _Fail the WebSocket Connection_
                    code = close_code::too_big;
                    ec = error::failed;
                    goto close;
                }
                cb_.consume(zs.total_out);
                ws_.rd_.size += zs.total_out;
                ws_.rd_.remain -= zs.total_in;
                ws_.rd_.buf.consume(zs.total_in);
                bytes_written_ += zs.total_out;
            }
            if(ws_.rd_.op == detail::opcode::text)
            {
                // check utf8
                if(! ws_.rd_.utf8.write(
                    buffer_prefix(bytes_written_, cb_.get())) || (
                        ws_.rd_.done && ! ws_.rd_.utf8.finish()))
                {
                    // _Fail the WebSocket Connection_
                    code = close_code::bad_payload;
                    ec = error::failed;
                    goto close;
                }
            }
            goto upcall;
        }

    close:
        // Maybe send close frame, then teardown
        BOOST_ASIO_CORO_YIELD
        ws_.do_async_fail(code, ec, std::move(*this));
        //BOOST_ASSERT(! ws_.wr_block_);
        goto upcall;

    upcall:
        BOOST_ASSERT(ws_.rd_block_ == tok_);
        ws_.rd_block_.reset();
        ws_.r_close_op_.maybe_invoke();
        ws_.close_op_.maybe_invoke() ||
            ws_.ping_op_.maybe_invoke() ||
            ws_.wr_op_.maybe_invoke();
        if(! dispatched_)
        {
            ws_.stream_.get_io_service().post(
                bind_handler(std::move(h_),
                    ec, bytes_written_));
        }
        else
        {
            h_(ec, bytes_written_);
        }
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<
    class DynamicBuffer,
    class Handler>
class stream<NextLayer>::read_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    DynamicBuffer& b_;
    std::size_t limit_;
    std::size_t bytes_written_ = 0;
    bool some_;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = default;

    template<class DeducedHandler>
    read_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        DynamicBuffer& b,
        std::size_t limit,
        bool some)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , b_(b)
        , limit_(limit ? limit : (
            std::numeric_limits<std::size_t>::max)())
        , some_(some)
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class DynamicBuffer, class Handler>
void
stream<NextLayer>::
read_op<DynamicBuffer, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    using beast::detail::clamp;
    using buffers_type = typename
        DynamicBuffer::mutable_buffers_type;
    boost::optional<buffers_type> mb;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        do
        {
            try
            {
                mb.emplace(b_.prepare(clamp(
                    ws_.read_size_hint(b_), limit_)));
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
            }
            if(ec)
            {
                BOOST_ASIO_CORO_YIELD
                ws_.get_io_service().post(
                    bind_handler(std::move(*this),
                        error::buffer_overflow, 0));
                break;
            }
            BOOST_ASIO_CORO_YIELD
            read_some_op<buffers_type, read_op>{
                std::move(*this), ws_, *mb}();
            if(ec)
                break;
            b_.commit(bytes_transferred);
            bytes_written_ += bytes_transferred;
        }
        while(! some_ && ! ws_.is_message_done());
        h_(ec, bytes_written_);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read(DynamicBuffer& buffer)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written = read(buffer, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read(DynamicBuffer& buffer, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    std::size_t bytes_written = 0;
    do
    {
        bytes_written += read_some(buffer, 0, ec);
        if(ec)
            return bytes_written;
    }
    while(! is_message_done());
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
async_return_type<ReadHandler, void(error_code, std::size_t)>
stream<NextLayer>::
async_read(DynamicBuffer& buffer, ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    async_completion<
        ReadHandler, void(error_code, std::size_t)> init{handler};
    read_op<
        DynamicBuffer,
        handler_type<ReadHandler, void(error_code, std::size_t)> >{
            init.completion_handler,
            *this,
            buffer,
            0,
            false}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written =
        read_some(buffer, limit, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    using beast::detail::clamp;
    if(! limit)
        limit = (std::numeric_limits<std::size_t>::max)();
    auto const size =
        clamp(read_size_hint(buffer), limit);
    BOOST_ASSERT(size > 0);
    boost::optional<typename
        DynamicBuffer::mutable_buffers_type> mb;
    try
    {
        mb.emplace(buffer.prepare(size));
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return 0;
    }
    auto const bytes_written = read_some(*mb, ec);
    buffer.commit(bytes_written);
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
async_return_type<ReadHandler,
    void(error_code, std::size_t)>
stream<NextLayer>::
async_read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    read_op<
        DynamicBuffer,
        handler_type<ReadHandler,
            void(error_code, std::size_t)>>{
        init.completion_handler,
        *this,
        buffer,
        limit,
        true}({}, 0);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer>::
read_some(
    MutableBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    error_code ec;
    auto const bytes_written = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer>::
read_some(
    MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    close_code code{};
    std::size_t bytes_written = 0;
    // Make sure the stream is open
    if(! open_)
    {
        ec = boost::asio::error::operation_aborted;
        return 0;
    }
loop:
    // See if we need to read a frame header. This
    // condition is structured to give the decompressor
    // a chance to emit the final empty deflate block
    //
    if(rd_.remain == 0 && (! rd_.fh.fin || rd_.done))
    {
        // Read frame header
        while(! parse_fh(rd_.fh, rd_.buf, code))
        {
            if(code != close_code::none)
            {
                // _Fail the WebSocket Connection_
                do_fail(code, error::failed, ec);
                return bytes_written;
            }
            auto const bytes_transferred =
                stream_.read_some(
                    rd_.buf.prepare(read_size(
                        rd_.buf, rd_.buf.max_size())),
                    ec);
            open_ = ! ec;
            if(! open_)
                return bytes_written;
            rd_.buf.commit(bytes_transferred);
        }
        // Immediately apply the mask to the portion
        // of the buffer holding payload data.
        if(rd_.fh.len > 0 && rd_.fh.mask)
            detail::mask_inplace(buffer_prefix(
                clamp(rd_.fh.len), rd_.buf.data()),
                    rd_.key);
        if(detail::is_control(rd_.fh.op))
        {
            // Get control frame payload
            auto const b = buffer_prefix(
                clamp(rd_.fh.len), rd_.buf.data());
            auto const len = buffer_size(b);
            BOOST_ASSERT(len == rd_.fh.len);

            // Clear this otherwise the next
            // frame will be considered final.
            rd_.fh.fin = false;

            // Handle ping frame
            if(rd_.fh.op == detail::opcode::ping)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                rd_.buf.consume(len);
                if(wr_close_)
                {
                    // Ignore ping when closing
                    goto loop;
                }
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::ping, payload);
                detail::frame_buffer fb;
                write_ping<flat_static_buffer_base>(fb,
                    detail::opcode::pong, payload);
                boost::asio::write(stream_, fb.data(), ec);
                open_ = ! ec;
                if(! open_)
                    return bytes_written;
                goto loop;
            }
            // Handle pong frame
            if(rd_.fh.op == detail::opcode::pong)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                rd_.buf.consume(len);
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::pong, payload);
                goto loop;
            }
            // Handle close frame
            BOOST_ASSERT(rd_.fh.op == detail::opcode::close);
            {
                BOOST_ASSERT(! rd_close_);
                rd_close_ = true;
                close_reason cr;
                detail::read_close(cr, b, code);
                if(code != close_code::none)
                {
                    // _Fail the WebSocket Connection_
                    do_fail(code, error::failed, ec);
                    return bytes_written;
                }
                cr_ = cr;
                rd_.buf.consume(len);
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::close, cr_.reason);
                BOOST_ASSERT(! wr_close_);
                // _Start the WebSocket Closing Handshake_
                do_fail(
                    cr.code == close_code::none ?
                        close_code::normal : cr.code,
                    error::closed, ec);
                return bytes_written;
            }
        }
        if(rd_.fh.len == 0 && ! rd_.fh.fin)
        {
            // Empty non-final frame
            goto loop;
        }
        rd_.done = false;
    }
    else
    {
        ec.assign(0, ec.category());
    }
    if(! pmd_ || ! pmd_->rd_set)
    {
        if(rd_.remain > 0)
        {
            if(rd_.buf.size() == 0 && rd_.buf.max_size() >
                (std::min)(clamp(rd_.remain),
                    buffer_size(buffers)))
            {
                // Fill the read buffer first, otherwise we
                // get fewer bytes at the cost of one I/O.
                rd_.buf.commit(stream_.read_some(
                    rd_.buf.prepare(read_size(rd_.buf,
                        rd_.buf.max_size())), ec));
                open_ = ! ec;
                if(! open_)
                    return bytes_written;
                if(rd_.fh.mask)
                    detail::mask_inplace(
                        buffer_prefix(clamp(rd_.remain),
                            rd_.buf.data()), rd_.key);
            }
            if(rd_.buf.size() > 0)
            {
                // Copy from the read buffer.
                // The mask was already applied.
                auto const bytes_transferred =
                    buffer_copy(buffers, rd_.buf.data(),
                        clamp(rd_.remain));
                auto const mb = buffer_prefix(
                    bytes_transferred, buffers);
                rd_.remain -= bytes_transferred;
                if(rd_.op == detail::opcode::text)
                {
                    if(! rd_.utf8.write(mb) ||
                        (rd_.remain == 0 && rd_.fh.fin &&
                            ! rd_.utf8.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(
                            close_code::bad_payload,
                            error::failed,
                            ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                rd_.size += bytes_transferred;
                rd_.buf.consume(bytes_transferred);
            }
            else
            {
                // Read into caller's buffer
                BOOST_ASSERT(rd_.remain > 0);
                BOOST_ASSERT(buffer_size(buffers) > 0);
                auto const bytes_transferred =
                    stream_.read_some(buffer_prefix(
                        clamp(rd_.remain), buffers), ec);
                open_ = ! ec;
                if(! open_)
                    return bytes_written;
                BOOST_ASSERT(bytes_transferred > 0);
                auto const mb = buffer_prefix(
                    bytes_transferred, buffers);
                rd_.remain -= bytes_transferred;
                if(rd_.fh.mask)
                    detail::mask_inplace(mb, rd_.key);
                if(rd_.op == detail::opcode::text)
                {
                    if(! rd_.utf8.write(mb) ||
                        (rd_.remain == 0 && rd_.fh.fin &&
                            ! rd_.utf8.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(close_code::bad_payload,
                            error::failed, ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                rd_.size += bytes_transferred;
            }
        }
        rd_.done = rd_.remain == 0 && rd_.fh.fin;
    }
    else
    {
        // Read compressed message frame payload:
        // inflate even if rd_.fh.len == 0, otherwise we
        // never emit the end-of-stream deflate block.
        //
        bool did_read = false;
        consuming_buffers<MutableBufferSequence> cb{buffers};
        while(buffer_size(cb) > 0)
        {
            zlib::z_params zs;
            {
                auto const out = buffer_front(cb);
                zs.next_out = buffer_cast<void*>(out);
                zs.avail_out = buffer_size(out);
                BOOST_ASSERT(zs.avail_out > 0);
            }
            if(rd_.remain > 0)
            {
                if(rd_.buf.size() > 0)
                {
                    // use what's there
                    auto const in = buffer_prefix(
                        clamp(rd_.remain), buffer_front(
                            rd_.buf.data()));
                    zs.avail_in = buffer_size(in);
                    zs.next_in = buffer_cast<void const*>(in);
                }
                else if(! did_read)
                {
                    // read new
                    auto const bytes_transferred =
                        stream_.read_some(
                            rd_.buf.prepare(read_size(
                                rd_.buf, rd_.buf.max_size())),
                            ec);
                    open_ = ! ec;
                    if(! open_)
                        return bytes_written;
                    BOOST_ASSERT(bytes_transferred > 0);
                    rd_.buf.commit(bytes_transferred);
                    if(rd_.fh.mask)
                        detail::mask_inplace(
                            buffer_prefix(clamp(rd_.remain),
                                rd_.buf.data()), rd_.key);
                    auto const in = buffer_prefix(
                        clamp(rd_.remain), buffer_front(
                            rd_.buf.data()));
                    zs.avail_in = buffer_size(in);
                    zs.next_in = buffer_cast<void const*>(in);
                    did_read = true;
                }
                else
                {
                    break;
                }
            }
            else if(rd_.fh.fin)
            {
                // append the empty block codes
                static std::uint8_t constexpr
                    empty_block[4] = {
                        0x00, 0x00, 0xff, 0xff };
                zs.next_in = empty_block;
                zs.avail_in = sizeof(empty_block);
                pmd_->zi.write(zs, zlib::Flush::sync, ec);
                BOOST_ASSERT(! ec);
                open_ = ! ec;
                if(! open_)
                    return bytes_written;
                // VFALCO See:
                // https://github.com/madler/zlib/issues/280
                BOOST_ASSERT(zs.total_out == 0);
                cb.consume(zs.total_out);
                rd_.size += zs.total_out;
                bytes_written += zs.total_out;
                if(
                    (role_ == role_type::client &&
                        pmd_config_.server_no_context_takeover) ||
                    (role_ == role_type::server &&
                        pmd_config_.client_no_context_takeover))
                    pmd_->zi.reset();
                rd_.done = true;
                break;
            }
            else
            {
                break;
            }
            pmd_->zi.write(zs, zlib::Flush::sync, ec);
            BOOST_ASSERT(ec != zlib::error::end_of_stream);
            open_ = ! ec;
            if(! open_)
                return bytes_written;
            if(rd_msg_max_ && beast::detail::sum_exceeds(
                rd_.size, zs.total_out, rd_msg_max_))
            {
                do_fail(close_code::too_big,
                    error::failed, ec);
                return bytes_written;
            }
            cb.consume(zs.total_out);
            rd_.size += zs.total_out;
            rd_.remain -= zs.total_in;
            rd_.buf.consume(zs.total_in);
            bytes_written += zs.total_out;
        }
        if(rd_.op == detail::opcode::text)
        {
            // check utf8
            if(! rd_.utf8.write(
                buffer_prefix(bytes_written, buffers)) || (
                    rd_.done && ! rd_.utf8.finish()))
            {
                // _Fail the WebSocket Connection_
                do_fail(close_code::bad_payload,
                    error::failed, ec);
                return bytes_written;
            }
        }
    }
    return bytes_written;
}

template<class NextLayer>
template<class MutableBufferSequence, class ReadHandler>
async_return_type<ReadHandler, void(error_code, std::size_t)>
stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    read_some_op<MutableBufferSequence, handler_type<
        ReadHandler, void(error_code, std::size_t)>>{
            init.completion_handler,*this, buffers}(
                {}, 0);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif