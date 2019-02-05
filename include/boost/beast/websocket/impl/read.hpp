//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_READ_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_READ_HPP

#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/buffer.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
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
template<class NextLayer, bool deflateSupported>
template<
    class MutableBufferSequence,
    class Handler>
class stream<NextLayer, deflateSupported>::read_some_op
    : public beast::async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    stream& ws_;
    MutableBufferSequence bs_;
    buffers_suffix<MutableBufferSequence> cb_;
    std::size_t bytes_written_ = 0;
    error_code result_;
    close_code code_;
    bool did_read_ = false;
    bool cont_ = false;

public:
    static constexpr int id = 1; // for soft_mutex

    template<class Handler_>
    read_some_op(
        Handler_&& h,
        stream<NextLayer, deflateSupported>& ws,
        MutableBufferSequence const& bs)
        : async_op_base<
            Handler, beast::executor_type<stream>>(
                std::forward<Handler_>(h), ws.get_executor())
        , ws_(ws)
        , bs_(bs)
        , cb_(bs)
        , code_(close_code::none)
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true)
    {
        using beast::detail::clamp;
        auto& impl = *ws_.impl_;
        cont_ = cont;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Maybe suspend
        do_maybe_suspend:
            if(impl.rd_block.try_lock(this))
            {
                // Make sure the stream is not closed
                if( impl.status_ == status::closed ||
                    impl.status_ == status::failed)
                {
                    ec = net::error::operation_aborted;
                    goto upcall;
                }
            }
            else
            {
            do_suspend:
                // Suspend
                BOOST_ASIO_CORO_YIELD
                impl.paused_r_rd.emplace(std::move(*this));

                // Acquire the read block
                impl.rd_block.lock(this);

                // Resume
                BOOST_ASIO_CORO_YIELD
                net::post(
                    ws_.get_executor(), std::move(*this));
                BOOST_ASSERT(impl.rd_block.is_locked(this));

                // The only way to get read blocked is if
                // a `close_op` wrote a close frame
                BOOST_ASSERT(impl.wr_close);
                BOOST_ASSERT(impl.status_ != status::open);
                ec = net::error::operation_aborted;
                goto upcall;
            }

            // if status_ == status::closing, we want to suspend
            // the read operation until the close completes,
            // then finish the read with operation_aborted.

        loop:
            BOOST_ASSERT(impl.rd_block.is_locked(this));
            // See if we need to read a frame header. This
            // condition is structured to give the decompressor
            // a chance to emit the final empty deflate block
            //
            if(impl.rd_remain == 0 &&
                (! impl.rd_fh.fin || impl.rd_done))
            {
                // Read frame header
                while(! ws_.parse_fh(
                    impl.rd_fh, impl.rd_buf, result_))
                {
                    if(result_)
                    {
                        // _Fail the WebSocket Connection_
                        if(result_ == error::message_too_big)
                            code_ = close_code::too_big;
                        else
                            code_ = close_code::protocol_error;
                        goto close;
                    }
                    BOOST_ASSERT(impl.rd_block.is_locked(this));
                    BOOST_ASIO_CORO_YIELD
                    impl.stream.async_read_some(
                        impl.rd_buf.prepare(read_size(
                            impl.rd_buf, impl.rd_buf.max_size())),
                                std::move(*this));
                    BOOST_ASSERT(impl.rd_block.is_locked(this));
                    if(! impl.check_ok(ec))
                        goto upcall;
                    impl.rd_buf.commit(bytes_transferred);

                    // Allow a close operation
                    // to acquire the read block
                    impl.rd_block.unlock(this);
                    if( impl.paused_r_close.maybe_invoke())
                    {
                        // Suspend
                        BOOST_ASSERT(impl.rd_block.is_locked());
                        goto do_suspend;
                    }
                    // Acquire read block
                    impl.rd_block.lock(this);
                }
                // Immediately apply the mask to the portion
                // of the buffer holding payload data.
                if(impl.rd_fh.len > 0 && impl.rd_fh.mask)
                    detail::mask_inplace(buffers_prefix(
                        clamp(impl.rd_fh.len),
                            impl.rd_buf.data()),
                                impl.rd_key);
                if(detail::is_control(impl.rd_fh.op))
                {
                    // Clear this otherwise the next
                    // frame will be considered final.
                    impl.rd_fh.fin = false;

                    // Handle ping frame
                    if(impl.rd_fh.op == detail::opcode::ping)
                    {
                        if(impl.ctrl_cb)
                        {
                            if(! cont_)
                            {
                                BOOST_ASIO_CORO_YIELD
                                net::post(
                                    ws_.get_executor(),
                                        std::move(*this));
                                BOOST_ASSERT(cont_);
                            }
                        }
                        {
                            auto const b = buffers_prefix(
                                clamp(impl.rd_fh.len),
                                    impl.rd_buf.data());
                            auto const len = buffer_size(b);
                            BOOST_ASSERT(len == impl.rd_fh.len);
                            ping_data payload;
                            detail::read_ping(payload, b);
                            impl.rd_buf.consume(len);
                            // Ignore ping when closing
                            if(impl.status_ == status::closing)
                                goto loop;
                            if(impl.ctrl_cb)
                                impl.ctrl_cb(
                                    frame_type::ping, payload);
                            impl.rd_fb.clear();
                            ws_.template write_ping<
                                flat_static_buffer_base>(impl.rd_fb,
                                    detail::opcode::pong, payload);
                        }

                        // Allow a close operation
                        // to acquire the read block
                        impl.rd_block.unlock(this);
                        impl.paused_r_close.maybe_invoke();

                        // Maybe suspend
                        if(! impl.wr_block.try_lock(this))
                        {
                            // Suspend
                            BOOST_ASIO_CORO_YIELD
                            impl.paused_rd.emplace(std::move(*this));

                            // Acquire the write block
                            impl.wr_block.lock(this);

                            // Resume
                            BOOST_ASIO_CORO_YIELD
                            net::post(
                                ws_.get_executor(), std::move(*this));
                            BOOST_ASSERT(impl.wr_block.is_locked(this));

                            // Make sure the stream is open
                            if(! impl.check_open(ec))
                                goto upcall;
                        }

                        // Send pong
                        BOOST_ASSERT(impl.wr_block.is_locked(this));
                        BOOST_ASIO_CORO_YIELD
                        net::async_write(impl.stream,
                            impl.rd_fb.data(), std::move(*this));
                        BOOST_ASSERT(impl.wr_block.is_locked(this));
                        if(! impl.check_ok(ec))
                            goto upcall;
                        impl.wr_block.unlock(this);
                        impl.paused_close.maybe_invoke() ||
                            impl.paused_ping.maybe_invoke() ||
                            impl.paused_wr.maybe_invoke();
                        goto do_maybe_suspend;
                    }
                    // Handle pong frame
                    if(impl.rd_fh.op == detail::opcode::pong)
                    {
                        // Ignore pong when closing
                        if(! impl.wr_close && impl.ctrl_cb)
                        {
                            if(! cont_)
                            {
                                BOOST_ASIO_CORO_YIELD
                                net::post(
                                    ws_.get_executor(),
                                        std::move(*this));
                                BOOST_ASSERT(cont_);
                            }
                        }
                        auto const cb = buffers_prefix(clamp(
                            impl.rd_fh.len), impl.rd_buf.data());
                        auto const len = buffer_size(cb);
                        BOOST_ASSERT(len == impl.rd_fh.len);
                        ping_data payload;
                        detail::read_ping(payload, cb);
                        impl.rd_buf.consume(len);
                        // Ignore pong when closing
                        if(! impl.wr_close && impl.ctrl_cb)
                            impl.ctrl_cb(frame_type::pong, payload);
                        goto loop;
                    }
                    // Handle close frame
                    BOOST_ASSERT(impl.rd_fh.op == detail::opcode::close);
                    {
                        if(impl.ctrl_cb)
                        {
                            if(! cont_)
                            {
                                BOOST_ASIO_CORO_YIELD
                                net::post(
                                    ws_.get_executor(),
                                        std::move(*this));
                                BOOST_ASSERT(cont_);
                            }
                        }
                        auto const cb = buffers_prefix(clamp(
                            impl.rd_fh.len), impl.rd_buf.data());
                        auto const len = buffer_size(cb);
                        BOOST_ASSERT(len == impl.rd_fh.len);
                        BOOST_ASSERT(! impl.rd_close);
                        impl.rd_close = true;
                        close_reason cr;
                        detail::read_close(cr, cb, result_);
                        if(result_)
                        {
                            // _Fail the WebSocket Connection_
                            code_ = close_code::protocol_error;
                            goto close;
                        }
                        impl.cr = cr;
                        impl.rd_buf.consume(len);
                        if(impl.ctrl_cb)
                            impl.ctrl_cb(frame_type::close,
                                impl.cr.reason);
                        // See if we are already closing
                        if(impl.status_ == status::closing)
                        {
                            // _Close the WebSocket Connection_
                            BOOST_ASSERT(impl.wr_close);
                            code_ = close_code::none;
                            result_ = error::closed;
                            goto close;
                        }
                        // _Start the WebSocket Closing Handshake_
                        code_ = cr.code == close_code::none ?
                            close_code::normal :
                            static_cast<close_code>(cr.code);
                        result_ = error::closed;
                        goto close;
                    }
                }
                if(impl.rd_fh.len == 0 && ! impl.rd_fh.fin)
                {
                    // Empty non-final frame
                    goto loop;
                }
                impl.rd_done = false;
            }
            if(! impl.rd_deflated())
            {
                if(impl.rd_remain > 0)
                {
                    if(impl.rd_buf.size() == 0 && impl.rd_buf.max_size() >
                        (std::min)(clamp(impl.rd_remain),
                            buffer_size(cb_)))
                    {
                        // Fill the read buffer first, otherwise we
                        // get fewer bytes at the cost of one I/O.
                        BOOST_ASIO_CORO_YIELD
                        impl.stream.async_read_some(
                            impl.rd_buf.prepare(read_size(
                                impl.rd_buf, impl.rd_buf.max_size())),
                                    std::move(*this));
                        if(! impl.check_ok(ec))
                            goto upcall;
                        impl.rd_buf.commit(bytes_transferred);
                        if(impl.rd_fh.mask)
                            detail::mask_inplace(buffers_prefix(clamp(
                                impl.rd_remain), impl.rd_buf.data()),
                                    impl.rd_key);
                    }
                    if(impl.rd_buf.size() > 0)
                    {
                        // Copy from the read buffer.
                        // The mask was already applied.
                        bytes_transferred = net::buffer_copy(cb_,
                            impl.rd_buf.data(), clamp(impl.rd_remain));
                        auto const mb = buffers_prefix(
                            bytes_transferred, cb_);
                        impl.rd_remain -= bytes_transferred;
                        if(impl.rd_op == detail::opcode::text)
                        {
                            if(! impl.rd_utf8.write(mb) ||
                                (impl.rd_remain == 0 && impl.rd_fh.fin &&
                                    ! impl.rd_utf8.finish()))
                            {
                                // _Fail the WebSocket Connection_
                                code_ = close_code::bad_payload;
                                result_ = error::bad_frame_payload;
                                goto close;
                            }
                        }
                        bytes_written_ += bytes_transferred;
                        impl.rd_size += bytes_transferred;
                        impl.rd_buf.consume(bytes_transferred);
                    }
                    else
                    {
                        // Read into caller's buffer
                        BOOST_ASSERT(impl.rd_remain > 0);
                        BOOST_ASSERT(buffer_size(cb_) > 0);
                        BOOST_ASSERT(buffer_size(buffers_prefix(
                            clamp(impl.rd_remain), cb_)) > 0);
                        BOOST_ASIO_CORO_YIELD
                        impl.stream.async_read_some(buffers_prefix(
                            clamp(impl.rd_remain), cb_), std::move(*this));
                        if(! impl.check_ok(ec))
                            goto upcall;
                        BOOST_ASSERT(bytes_transferred > 0);
                        auto const mb = buffers_prefix(
                            bytes_transferred, cb_);
                        impl.rd_remain -= bytes_transferred;
                        if(impl.rd_fh.mask)
                            detail::mask_inplace(mb, impl.rd_key);
                        if(impl.rd_op == detail::opcode::text)
                        {
                            if(! impl.rd_utf8.write(mb) ||
                                (impl.rd_remain == 0 && impl.rd_fh.fin &&
                                    ! impl.rd_utf8.finish()))
                            {
                                // _Fail the WebSocket Connection_
                                code_ = close_code::bad_payload;
                                result_ = error::bad_frame_payload;
                                goto close;
                            }
                        }
                        bytes_written_ += bytes_transferred;
                        impl.rd_size += bytes_transferred;
                    }
                }
                impl.rd_done = impl.rd_remain == 0 && impl.rd_fh.fin;
            }
            else
            {
                // Read compressed message frame payload:
                // inflate even if rd_fh_.len == 0, otherwise we
                // never emit the end-of-stream deflate block.
                while(buffer_size(cb_) > 0)
                {
                    if( impl.rd_remain > 0 &&
                        impl.rd_buf.size() == 0 &&
                        ! did_read_)
                    {
                        // read new
                        BOOST_ASIO_CORO_YIELD
                        impl.stream.async_read_some(
                            impl.rd_buf.prepare(read_size(
                                impl.rd_buf, impl.rd_buf.max_size())),
                                    std::move(*this));
                        if(! impl.check_ok(ec))
                            goto upcall;
                        BOOST_ASSERT(bytes_transferred > 0);
                        impl.rd_buf.commit(bytes_transferred);
                        if(impl.rd_fh.mask)
                            detail::mask_inplace(
                                buffers_prefix(clamp(impl.rd_remain),
                                    impl.rd_buf.data()), impl.rd_key);
                        did_read_ = true;
                    }
                    zlib::z_params zs;
                    {
                        auto const out = buffers_front(cb_);
                        zs.next_out = out.data();
                        zs.avail_out = out.size();
                        BOOST_ASSERT(zs.avail_out > 0);
                    }
                    if(impl.rd_remain > 0)
                    {
                        if(impl.rd_buf.size() > 0)
                        {
                            // use what's there
                            auto const in = buffers_prefix(
                                clamp(impl.rd_remain), buffers_front(
                                    impl.rd_buf.data()));
                            zs.avail_in = in.size();
                            zs.next_in = in.data();
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if(impl.rd_fh.fin)
                    {
                        // append the empty block codes
                        static std::uint8_t constexpr
                            empty_block[4] = {
                                0x00, 0x00, 0xff, 0xff };
                        zs.next_in = empty_block;
                        zs.avail_in = sizeof(empty_block);
                        impl.inflate(zs, zlib::Flush::sync, ec);
                        if(! ec)
                        {
                            // https://github.com/madler/zlib/issues/280
                            if(zs.total_out > 0)
                                ec = error::partial_deflate_block;
                        }
                        if(! impl.check_ok(ec))
                            goto upcall;
                        impl.do_context_takeover_read(impl.role);
                        impl.rd_done = true;
                        break;
                    }
                    else
                    {
                        break;
                    }
                    impl.inflate(zs, zlib::Flush::sync, ec);
                    if(! impl.check_ok(ec))
                        goto upcall;
                    if(impl.rd_msg_max && beast::detail::sum_exceeds(
                        impl.rd_size, zs.total_out, impl.rd_msg_max))
                    {
                        // _Fail the WebSocket Connection_
                        code_ = close_code::too_big;
                        result_ = error::message_too_big;
                        goto close;
                    }
                    cb_.consume(zs.total_out);
                    impl.rd_size += zs.total_out;
                    impl.rd_remain -= zs.total_in;
                    impl.rd_buf.consume(zs.total_in);
                    bytes_written_ += zs.total_out;
                }
                if(impl.rd_op == detail::opcode::text)
                {
                    // check utf8
                    if(! impl.rd_utf8.write(
                        buffers_prefix(bytes_written_, bs_)) || (
                            impl.rd_done && ! impl.rd_utf8.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        code_ = close_code::bad_payload;
                        result_ = error::bad_frame_payload;
                        goto close;
                    }
                }
            }
            goto upcall;

        close:
            // Try to acquire the write block
            if(! impl.wr_block.try_lock(this))
            {
                // Suspend
                BOOST_ASIO_CORO_YIELD
                impl.paused_rd.emplace(std::move(*this));

                // Acquire the write block
                impl.wr_block.lock(this);

                // Resume
                BOOST_ASIO_CORO_YIELD
                net::post(
                    ws_.get_executor(), std::move(*this));
                BOOST_ASSERT(impl.wr_block.is_locked(this));

                // Make sure the stream is open
                if(! impl.check_open(ec))
                    goto upcall;
            }

            // Set the status
            impl.status_ = status::closing;

            if(! impl.wr_close)
            {
                impl.wr_close = true;

                // Serialize close frame
                impl.rd_fb.clear();
                ws_.template write_close<
                    flat_static_buffer_base>(
                        impl.rd_fb, code_);

                // Send close frame
                BOOST_ASSERT(impl.wr_block.is_locked(this));
                BOOST_ASIO_CORO_YIELD
                net::async_write(
                    impl.stream, impl.rd_fb.data(),
                        std::move(*this));
                BOOST_ASSERT(impl.wr_block.is_locked(this));
                if(! impl.check_ok(ec))
                    goto upcall;
            }

            // Teardown
            using beast::websocket::async_teardown;
            BOOST_ASSERT(impl.wr_block.is_locked(this));
            BOOST_ASIO_CORO_YIELD
            async_teardown(impl.role,
                impl.stream, std::move(*this));
            BOOST_ASSERT(impl.wr_block.is_locked(this));
            if(ec == net::error::eof)
            {
                // Rationale:
                // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                ec = {};
            }
            if(! ec)
                ec = result_;
            if(ec && ec != error::closed)
                impl.status_ = status::failed;
            else
                impl.status_ = status::closed;
            impl.close();

        upcall:
            impl.rd_block.try_unlock(this);
            impl.paused_r_close.maybe_invoke();
            if(impl.wr_block.try_unlock(this))
                impl.paused_close.maybe_invoke() ||
                    impl.paused_ping.maybe_invoke() ||
                    impl.paused_wr.maybe_invoke();
            if(! cont_)
            {
                BOOST_ASIO_CORO_YIELD
                net::post(
                    ws_.get_executor(),
                    beast::bind_front_handler(std::move(*this),
                        ec, bytes_written_));
            }
            this->invoke(ec, bytes_written_);
        }
    }
};

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<
    class DynamicBuffer,
    class Handler>
class stream<NextLayer, deflateSupported>::read_op
    : public beast::async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    stream<NextLayer, deflateSupported>& ws_;
    DynamicBuffer& b_;
    std::size_t limit_;
    std::size_t bytes_written_ = 0;
    bool some_;

public:
    template<class Handler_>
    read_op(
        Handler_&& h,
        stream<NextLayer, deflateSupported>& ws,
        DynamicBuffer& b,
        std::size_t limit,
        bool some)
        : async_op_base<
            Handler, beast::executor_type<stream>>(
                std::forward<Handler_>(h), ws.get_executor())
        , ws_(ws)
        , b_(b)
        , limit_(limit ? limit : (
            std::numeric_limits<std::size_t>::max)())
        , some_(some)
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        using beast::detail::clamp;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            do
            {
                BOOST_ASIO_CORO_YIELD
                {
                    auto mb = beast::detail::dynamic_buffer_prepare(b_,
                        clamp(ws_.read_size_hint(b_), limit_),
                            ec, error::buffer_overflow);
                    if(ec)
                        net::post(
                            ws_.get_executor(),
                            beast::bind_front_handler(
                                std::move(*this), ec, 0));
                    else
                        read_some_op<typename
                            DynamicBuffer::mutable_buffers_type,
                                read_op>(std::move(*this), ws_, *mb)(
                                    {}, 0, false);
                }
                if(ec)
                    goto upcall;
                b_.commit(bytes_transferred);
                bytes_written_ += bytes_transferred;
            }
            while(! some_ && ! ws_.is_message_done());
        upcall:
            this->invoke(ec, bytes_written_);
        }
    }
};

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
std::size_t
stream<NextLayer, deflateSupported>::
read(DynamicBuffer& buffer)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written = read(buffer, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
std::size_t
stream<NextLayer, deflateSupported>::
read(DynamicBuffer& buffer, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
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

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer, deflateSupported>::
async_read(DynamicBuffer& buffer, ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_op<
        DynamicBuffer,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler),
                *this,
                buffer,
                0,
                false}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
std::size_t
stream<NextLayer, deflateSupported>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written =
        read_some(buffer, limit, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
std::size_t
stream<NextLayer, deflateSupported>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    using beast::detail::clamp;
    if(! limit)
        limit = (std::numeric_limits<std::size_t>::max)();
    auto const size =
        clamp(read_size_hint(buffer), limit);
    BOOST_ASSERT(size > 0);
    auto mb = beast::detail::dynamic_buffer_prepare(
        buffer, size, ec, error::buffer_overflow);
    if(ec)
        return 0;
    auto const bytes_written = read_some(*mb, ec);
    buffer.commit(bytes_written);
    return bytes_written;
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer, deflateSupported>::
async_read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_op<
        DynamicBuffer,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler),
                *this,
                buffer,
                limit,
                true}({}, 0);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
read_some(
    MutableBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    error_code ec;
    auto const bytes_written = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer, bool deflateSupported>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
read_some(
    MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using beast::detail::clamp;
    close_code code{};
    std::size_t bytes_written = 0;
    ec = {};
    // Make sure the stream is open
    if(! impl_->check_open(ec))
        return 0;
loop:
    // See if we need to read a frame header. This
    // condition is structured to give the decompressor
    // a chance to emit the final empty deflate block
    //
    if(impl_->rd_remain == 0 && (
        ! impl_->rd_fh.fin || impl_->rd_done))
    {
        // Read frame header
        error_code result;
        while(! parse_fh(impl_->rd_fh, impl_->rd_buf, result))
        {
            if(result)
            {
                // _Fail the WebSocket Connection_
                if(result == error::message_too_big)
                    code = close_code::too_big;
                else
                    code = close_code::protocol_error;
                do_fail(code, result, ec);
                return bytes_written;
            }
            auto const bytes_transferred =
                impl_->stream.read_some(
                    impl_->rd_buf.prepare(read_size(
                        impl_->rd_buf, impl_->rd_buf.max_size())),
                    ec);
            if(! impl_->check_ok(ec))
                return bytes_written;
            impl_->rd_buf.commit(bytes_transferred);
        }
        // Immediately apply the mask to the portion
        // of the buffer holding payload data.
        if(impl_->rd_fh.len > 0 && impl_->rd_fh.mask)
            detail::mask_inplace(buffers_prefix(
                clamp(impl_->rd_fh.len), impl_->rd_buf.data()),
                    impl_->rd_key);
        if(detail::is_control(impl_->rd_fh.op))
        {
            // Get control frame payload
            auto const b = buffers_prefix(
                clamp(impl_->rd_fh.len), impl_->rd_buf.data());
            auto const len = buffer_size(b);
            BOOST_ASSERT(len == impl_->rd_fh.len);

            // Clear this otherwise the next
            // frame will be considered final.
            impl_->rd_fh.fin = false;

            // Handle ping frame
            if(impl_->rd_fh.op == detail::opcode::ping)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                impl_->rd_buf.consume(len);
                if(impl_->wr_close)
                {
                    // Ignore ping when closing
                    goto loop;
                }
                if(impl_->ctrl_cb)
                    impl_->ctrl_cb(frame_type::ping, payload);
                detail::frame_buffer fb;
                write_ping<flat_static_buffer_base>(fb,
                    detail::opcode::pong, payload);
                net::write(impl_->stream, fb.data(), ec);
                if(! impl_->check_ok(ec))
                    return bytes_written;
                goto loop;
            }
            // Handle pong frame
            if(impl_->rd_fh.op == detail::opcode::pong)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                impl_->rd_buf.consume(len);
                if(impl_->ctrl_cb)
                    impl_->ctrl_cb(frame_type::pong, payload);
                goto loop;
            }
            // Handle close frame
            BOOST_ASSERT(impl_->rd_fh.op == detail::opcode::close);
            {
                BOOST_ASSERT(! impl_->rd_close);
                impl_->rd_close = true;
                close_reason cr;
                detail::read_close(cr, b, result);
                if(result)
                {
                    // _Fail the WebSocket Connection_
                    do_fail(close_code::protocol_error,
                        result, ec);
                    return bytes_written;
                }
                impl_->cr = cr;
                impl_->rd_buf.consume(len);
                if(impl_->ctrl_cb)
                    impl_->ctrl_cb(frame_type::close, impl_->cr.reason);
                BOOST_ASSERT(! impl_->wr_close);
                // _Start the WebSocket Closing Handshake_
                do_fail(
                    cr.code == close_code::none ?
                        close_code::normal :
                        static_cast<close_code>(cr.code),
                    error::closed, ec);
                return bytes_written;
            }
        }
        if(impl_->rd_fh.len == 0 && ! impl_->rd_fh.fin)
        {
            // Empty non-final frame
            goto loop;
        }
        impl_->rd_done = false;
    }
    else
    {
        ec = {};
    }
    if(! impl_->rd_deflated())
    {
        if(impl_->rd_remain > 0)
        {
            if(impl_->rd_buf.size() == 0 && impl_->rd_buf.max_size() >
                (std::min)(clamp(impl_->rd_remain),
                    buffer_size(buffers)))
            {
                // Fill the read buffer first, otherwise we
                // get fewer bytes at the cost of one I/O.
                impl_->rd_buf.commit(impl_->stream.read_some(
                    impl_->rd_buf.prepare(read_size(impl_->rd_buf,
                        impl_->rd_buf.max_size())), ec));
                if(! impl_->check_ok(ec))
                    return bytes_written;
                if(impl_->rd_fh.mask)
                    detail::mask_inplace(
                        buffers_prefix(clamp(impl_->rd_remain),
                            impl_->rd_buf.data()), impl_->rd_key);
            }
            if(impl_->rd_buf.size() > 0)
            {
                // Copy from the read buffer.
                // The mask was already applied.
                auto const bytes_transferred = net::buffer_copy(
                    buffers, impl_->rd_buf.data(),
                        clamp(impl_->rd_remain));
                auto const mb = buffers_prefix(
                    bytes_transferred, buffers);
                impl_->rd_remain -= bytes_transferred;
                if(impl_->rd_op == detail::opcode::text)
                {
                    if(! impl_->rd_utf8.write(mb) ||
                        (impl_->rd_remain == 0 && impl_->rd_fh.fin &&
                            ! impl_->rd_utf8.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(close_code::bad_payload,
                            error::bad_frame_payload, ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                impl_->rd_size += bytes_transferred;
                impl_->rd_buf.consume(bytes_transferred);
            }
            else
            {
                // Read into caller's buffer
                BOOST_ASSERT(impl_->rd_remain > 0);
                BOOST_ASSERT(buffer_size(buffers) > 0);
                BOOST_ASSERT(buffer_size(buffers_prefix(
                    clamp(impl_->rd_remain), buffers)) > 0);
                auto const bytes_transferred =
                    impl_->stream.read_some(buffers_prefix(
                        clamp(impl_->rd_remain), buffers), ec);
                if(! impl_->check_ok(ec))
                    return bytes_written;
                BOOST_ASSERT(bytes_transferred > 0);
                auto const mb = buffers_prefix(
                    bytes_transferred, buffers);
                impl_->rd_remain -= bytes_transferred;
                if(impl_->rd_fh.mask)
                    detail::mask_inplace(mb, impl_->rd_key);
                if(impl_->rd_op == detail::opcode::text)
                {
                    if(! impl_->rd_utf8.write(mb) ||
                        (impl_->rd_remain == 0 && impl_->rd_fh.fin &&
                            ! impl_->rd_utf8.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(close_code::bad_payload,
                            error::bad_frame_payload, ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                impl_->rd_size += bytes_transferred;
            }
        }
        impl_->rd_done = impl_->rd_remain == 0 && impl_->rd_fh.fin;
    }
    else
    {
        // Read compressed message frame payload:
        // inflate even if rd_fh_.len == 0, otherwise we
        // never emit the end-of-stream deflate block.
        //
        bool did_read = false;
        buffers_suffix<MutableBufferSequence> cb(buffers);
        while(buffer_size(cb) > 0)
        {
            zlib::z_params zs;
            {
                auto const out = beast::buffers_front(cb);
                zs.next_out = out.data();
                zs.avail_out = out.size();
                BOOST_ASSERT(zs.avail_out > 0);
            }
            if(impl_->rd_remain > 0)
            {
                if(impl_->rd_buf.size() > 0)
                {
                    // use what's there
                    auto const in = buffers_prefix(
                        clamp(impl_->rd_remain), beast::buffers_front(
                            impl_->rd_buf.data()));
                    zs.avail_in = in.size();
                    zs.next_in = in.data();
                }
                else if(! did_read)
                {
                    // read new
                    auto const bytes_transferred =
                        impl_->stream.read_some(
                            impl_->rd_buf.prepare(read_size(
                                impl_->rd_buf, impl_->rd_buf.max_size())),
                            ec);
                    if(! impl_->check_ok(ec))
                        return bytes_written;
                    BOOST_ASSERT(bytes_transferred > 0);
                    impl_->rd_buf.commit(bytes_transferred);
                    if(impl_->rd_fh.mask)
                        detail::mask_inplace(
                            buffers_prefix(clamp(impl_->rd_remain),
                                impl_->rd_buf.data()), impl_->rd_key);
                    auto const in = buffers_prefix(
                        clamp(impl_->rd_remain), buffers_front(
                            impl_->rd_buf.data()));
                    zs.avail_in = in.size();
                    zs.next_in = in.data();
                    did_read = true;
                }
                else
                {
                    break;
                }
            }
            else if(impl_->rd_fh.fin)
            {
                // append the empty block codes
                static std::uint8_t constexpr
                    empty_block[4] = {
                        0x00, 0x00, 0xff, 0xff };
                zs.next_in = empty_block;
                zs.avail_in = sizeof(empty_block);
                impl_->inflate(zs, zlib::Flush::sync, ec);
                if(! ec)
                {
                    // https://github.com/madler/zlib/issues/280
                    if(zs.total_out > 0)
                        ec = error::partial_deflate_block;
                }
                if(! impl_->check_ok(ec))
                    return bytes_written;
                impl_->do_context_takeover_read(impl_->role);
                impl_->rd_done = true;
                break;
            }
            else
            {
                break;
            }
            impl_->inflate(zs, zlib::Flush::sync, ec);
            if(! impl_->check_ok(ec))
                return bytes_written;
            if(impl_->rd_msg_max && beast::detail::sum_exceeds(
                impl_->rd_size, zs.total_out, impl_->rd_msg_max))
            {
                do_fail(close_code::too_big,
                    error::message_too_big, ec);
                return bytes_written;
            }
            cb.consume(zs.total_out);
            impl_->rd_size += zs.total_out;
            impl_->rd_remain -= zs.total_in;
            impl_->rd_buf.consume(zs.total_in);
            bytes_written += zs.total_out;
        }
        if(impl_->rd_op == detail::opcode::text)
        {
            // check utf8
            if(! impl_->rd_utf8.write(beast::buffers_prefix(
                bytes_written, buffers)) || (
                    impl_->rd_done && ! impl_->rd_utf8.finish()))
            {
                // _Fail the WebSocket Connection_
                do_fail(close_code::bad_payload,
                    error::bad_frame_payload, ec);
                return bytes_written;
            }
        }
    }
    return bytes_written;
}

template<class NextLayer, bool deflateSupported>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer, deflateSupported>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_some_op<MutableBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t))>{
            std::move(init.completion_handler), *this, buffers}(
                {}, 0, false);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
