//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_WRITE_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_WRITE_HPP

#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_range.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

template<class NextLayer, bool deflateSupported>
template<class Buffers, class Handler>
class stream<NextLayer, deflateSupported>::write_some_op
    : public beast::async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    stream& ws_;
    buffers_suffix<Buffers> cb_;
    detail::frame_header fh_;
    detail::prepared_key key_;
    std::size_t bytes_transferred_ = 0;
    std::size_t remain_;
    std::size_t in_;
    int how_;
    bool fin_;
    bool more_ = false; // for ubsan
    bool cont_ = false;

public:
    static constexpr int id = 2; // for soft_mutex

    template<class Handler_>
    write_some_op(
        Handler_&& h,
        stream<NextLayer, deflateSupported>& ws,
        bool fin,
        Buffers const& bs)
        : beast::async_op_base<Handler,
            beast::executor_type<stream>>(
                std::forward<Handler_>(h), ws.get_executor())
        , ws_(ws)
        , cb_(bs)
        , fin_(fin)
    {
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true);
};

template<class NextLayer, bool deflateSupported>
template<class Buffers, class Handler>
void
stream<NextLayer, deflateSupported>::
write_some_op<Buffers, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    using beast::detail::clamp;
    enum
    {
        do_nomask_nofrag,
        do_nomask_frag,
        do_mask_nofrag,
        do_mask_frag,
        do_deflate
    };
    std::size_t n;
    net::mutable_buffer b;
    cont_ = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Set up the outgoing frame header
        if(! ws_.impl_->wr_cont)
        {
            ws_.impl_->begin_msg();
            fh_.rsv1 = ws_.impl_->wr_compress;
        }
        else
        {
            fh_.rsv1 = false;
        }
        fh_.rsv2 = false;
        fh_.rsv3 = false;
        fh_.op = ws_.impl_->wr_cont ?
            detail::opcode::cont : ws_.impl_->wr_opcode;
        fh_.mask =
            ws_.impl_->role == role_type::client;

        // Choose a write algorithm
        if(ws_.impl_->wr_compress)
        {
            how_ = do_deflate;
        }
        else if(! fh_.mask)
        {
            if(! ws_.impl_->wr_frag)
            {
                how_ = do_nomask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.impl_->wr_buf_size != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.impl_->wr_buf_size)
                    how_ = do_nomask_frag;
                else
                    how_ = do_nomask_nofrag;
            }
        }
        else
        {
            if(! ws_.impl_->wr_frag)
            {
                how_ = do_mask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.impl_->wr_buf_size != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.impl_->wr_buf_size)
                    how_ = do_mask_frag;
                else
                    how_ = do_mask_nofrag;
            }
        }

        // Maybe suspend
        if(ws_.impl_->wr_block.try_lock(this))
        {
            // Make sure the stream is open
            if(! ws_.impl_->check_open(ec))
                goto upcall;
        }
        else
        {
        do_suspend:
            // Suspend
            BOOST_ASIO_CORO_YIELD
            ws_.impl_->paused_wr.emplace(std::move(*this));

            // Acquire the write block
            ws_.impl_->wr_block.lock(this);

            // Resume
            BOOST_ASIO_CORO_YIELD
            net::post(
                ws_.get_executor(), std::move(*this));
            BOOST_ASSERT(ws_.impl_->wr_block.is_locked(this));

            // Make sure the stream is open
            if(! ws_.impl_->check_open(ec))
                goto upcall;
        }

        //------------------------------------------------------------------

        if(how_ == do_nomask_nofrag)
        {
            fh_.fin = fin_;
            fh_.len = buffer_size(cb_);
            ws_.impl_->wr_fb.clear();
            detail::write<flat_static_buffer_base>(
                ws_.impl_->wr_fb, fh_);
            ws_.impl_->wr_cont = ! fin_;
            // Send frame
            BOOST_ASIO_CORO_YIELD
            net::async_write(ws_.impl_->stream,
                buffers_cat(ws_.impl_->wr_fb.data(), cb_),
                    std::move(*this));
            if(! ws_.impl_->check_ok(ec))
                goto upcall;
            bytes_transferred_ += clamp(fh_.len);
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_nomask_frag)
        {
            for(;;)
            {
                n = clamp(remain_, ws_.impl_->wr_buf_size);
                fh_.len = n;
                remain_ -= n;
                fh_.fin = fin_ ? remain_ == 0 : false;
                ws_.impl_->wr_fb.clear();
                detail::write<flat_static_buffer_base>(
                    ws_.impl_->wr_fb, fh_);
                ws_.impl_->wr_cont = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                net::async_write(
                    ws_.impl_->stream, buffers_cat(
                        ws_.impl_->wr_fb.data(), buffers_prefix(
                            clamp(fh_.len), cb_)),
                                std::move(*this));
                if(! ws_.impl_->check_ok(ec))
                    goto upcall;
                n = clamp(fh_.len); // because yield
                bytes_transferred_ += n;
                if(remain_ == 0)
                    break;
                cb_.consume(n);
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames
                ws_.impl_->wr_block.unlock(this);
                if( ws_.impl_->paused_close.maybe_invoke() ||
                    ws_.impl_->paused_rd.maybe_invoke() ||
                    ws_.impl_->paused_ping.maybe_invoke())
                {
                    BOOST_ASSERT(ws_.impl_->wr_block.is_locked());
                    goto do_suspend;
                }
                ws_.impl_->wr_block.lock(this);
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_nofrag)
        {
            remain_ = buffer_size(cb_);
            fh_.fin = fin_;
            fh_.len = remain_;
            fh_.key = ws_.create_mask();
            detail::prepare_key(key_, fh_.key);
            ws_.impl_->wr_fb.clear();
            detail::write<flat_static_buffer_base>(
                ws_.impl_->wr_fb, fh_);
            n = clamp(remain_, ws_.impl_->wr_buf_size);
            net::buffer_copy(net::buffer(
                ws_.impl_->wr_buf.get(), n), cb_);
            detail::mask_inplace(net::buffer(
                ws_.impl_->wr_buf.get(), n), key_);
            remain_ -= n;
            ws_.impl_->wr_cont = ! fin_;
            // Send frame header and partial payload
            BOOST_ASIO_CORO_YIELD
            net::async_write(
                ws_.impl_->stream, buffers_cat(ws_.impl_->wr_fb.data(),
                    net::buffer(ws_.impl_->wr_buf.get(), n)),
                        std::move(*this));
            if(! ws_.impl_->check_ok(ec))
                goto upcall;
            bytes_transferred_ +=
                bytes_transferred - ws_.impl_->wr_fb.size();
            while(remain_ > 0)
            {
                cb_.consume(ws_.impl_->wr_buf_size);
                n = clamp(remain_, ws_.impl_->wr_buf_size);
                net::buffer_copy(net::buffer(
                    ws_.impl_->wr_buf.get(), n), cb_);
                detail::mask_inplace(net::buffer(
                    ws_.impl_->wr_buf.get(), n), key_);
                remain_ -= n;
                // Send partial payload
                BOOST_ASIO_CORO_YIELD
                net::async_write(ws_.impl_->stream,
                    net::buffer(ws_.impl_->wr_buf.get(), n),
                        std::move(*this));
                if(! ws_.impl_->check_ok(ec))
                    goto upcall;
                bytes_transferred_ += bytes_transferred;
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_frag)
        {
            for(;;)
            {
                n = clamp(remain_, ws_.impl_->wr_buf_size);
                remain_ -= n;
                fh_.len = n;
                fh_.key = ws_.create_mask();
                fh_.fin = fin_ ? remain_ == 0 : false;
                detail::prepare_key(key_, fh_.key);
                net::buffer_copy(net::buffer(
                    ws_.impl_->wr_buf.get(), n), cb_);
                detail::mask_inplace(net::buffer(
                    ws_.impl_->wr_buf.get(), n), key_);
                ws_.impl_->wr_fb.clear();
                detail::write<flat_static_buffer_base>(
                    ws_.impl_->wr_fb, fh_);
                ws_.impl_->wr_cont = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                net::async_write(ws_.impl_->stream,
                    buffers_cat(ws_.impl_->wr_fb.data(),
                        net::buffer(ws_.impl_->wr_buf.get(), n)),
                            std::move(*this));
                if(! ws_.impl_->check_ok(ec))
                    goto upcall;
                n = bytes_transferred - ws_.impl_->wr_fb.size();
                bytes_transferred_ += n;
                if(remain_ == 0)
                    break;
                cb_.consume(n);
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames:
                ws_.impl_->wr_block.unlock(this);
                if( ws_.impl_->paused_close.maybe_invoke() ||
                    ws_.impl_->paused_rd.maybe_invoke() ||
                    ws_.impl_->paused_ping.maybe_invoke())
                {
                    BOOST_ASSERT(ws_.impl_->wr_block.is_locked());
                    goto do_suspend;
                }
                ws_.impl_->wr_block.lock(this);
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_deflate)
        {
            for(;;)
            {
                b = net::buffer(ws_.impl_->wr_buf.get(),
                    ws_.impl_->wr_buf_size);
                more_ = ws_.impl_->deflate(b, cb_, fin_, in_, ec);
                if(! ws_.impl_->check_ok(ec))
                    goto upcall;
                n = buffer_size(b);
                if(n == 0)
                {
                    // The input was consumed, but there
                    // is no output due to compression
                    // latency.
                    BOOST_ASSERT(! fin_);
                    BOOST_ASSERT(buffer_size(cb_) == 0);
                    goto upcall;
                }
                if(fh_.mask)
                {
                    fh_.key = ws_.create_mask();
                    detail::prepared_key key;
                    detail::prepare_key(key, fh_.key);
                    detail::mask_inplace(b, key);
                }
                fh_.fin = ! more_;
                fh_.len = n;
                ws_.impl_->wr_fb.clear();
                detail::write<
                    flat_static_buffer_base>(ws_.impl_->wr_fb, fh_);
                ws_.impl_->wr_cont = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                net::async_write(ws_.impl_->stream,
                    buffers_cat(ws_.impl_->wr_fb.data(), b),
                        std::move(*this));
                if(! ws_.impl_->check_ok(ec))
                    goto upcall;
                bytes_transferred_ += in_;
                if(more_)
                {
                    fh_.op = detail::opcode::cont;
                    fh_.rsv1 = false;
                    // Allow outgoing control frames to
                    // be sent in between message frames:
                    ws_.impl_->wr_block.unlock(this);
                    if( ws_.impl_->paused_close.maybe_invoke() ||
                        ws_.impl_->paused_rd.maybe_invoke() ||
                        ws_.impl_->paused_ping.maybe_invoke())
                    {
                        BOOST_ASSERT(ws_.impl_->wr_block.is_locked());
                        goto do_suspend;
                    }
                    ws_.impl_->wr_block.lock(this);
                }
                else
                {
                    if(fh_.fin)
                        ws_.impl_->do_context_takeover_write(ws_.impl_->role);
                    goto upcall;
                }
            }
        }

    //--------------------------------------------------------------------------

    upcall:
        ws_.impl_->wr_block.unlock(this);
        ws_.impl_->paused_close.maybe_invoke() ||
            ws_.impl_->paused_rd.maybe_invoke() ||
            ws_.impl_->paused_ping.maybe_invoke();
        if(! cont_)
        {
            BOOST_ASIO_CORO_YIELD
            net::post(
                ws_.get_executor(),
                beast::bind_front_handler(
                    std::move(*this),
                    ec, bytes_transferred_));
        }
        this->invoke_now(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
write_some(bool fin, ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(fin, buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
write_some(bool fin,
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using beast::detail::clamp;
    std::size_t bytes_transferred = 0;
    ec = {};
    // Make sure the stream is open
    if(! impl_->check_open(ec))
        return bytes_transferred;
    detail::frame_header fh;
    if(! impl_->wr_cont)
    {
        impl_->begin_msg();
        fh.rsv1 = impl_->wr_compress;
    }
    else
    {
        fh.rsv1 = false;
    }
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.op = impl_->wr_cont ?
        detail::opcode::cont : impl_->wr_opcode;
    fh.mask = impl_->role == role_type::client;
    auto remain = buffer_size(buffers);
    if(impl_->wr_compress)
    {
        buffers_suffix<
            ConstBufferSequence> cb{buffers};
        for(;;)
        {
            auto b = net::buffer(
                impl_->wr_buf.get(), impl_->wr_buf_size);
            auto const more = impl_->deflate(
                b, cb, fin, bytes_transferred, ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            auto const n = buffer_size(b);
            if(n == 0)
            {
                // The input was consumed, but there
                // is no output due to compression
                // latency.
                BOOST_ASSERT(! fin);
                BOOST_ASSERT(buffer_size(cb) == 0);
                fh.fin = false;
                break;
            }
            if(fh.mask)
            {
                fh.key = this->create_mask();
                detail::prepared_key key;
                detail::prepare_key(key, fh.key);
                detail::mask_inplace(b, key);
            }
            fh.fin = ! more;
            fh.len = n;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            impl_->wr_cont = ! fin;
            net::write(impl_->stream,
                buffers_cat(fh_buf.data(), b), ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            if(! more)
                break;
            fh.op = detail::opcode::cont;
            fh.rsv1 = false;
        }
        if(fh.fin)
            impl_->do_context_takeover_write(impl_->role);
    }
    else if(! fh.mask)
    {
        if(! impl_->wr_frag)
        {
            // no mask, no autofrag
            fh.fin = fin;
            fh.len = remain;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            impl_->wr_cont = ! fin;
            net::write(impl_->stream,
                buffers_cat(fh_buf.data(), buffers), ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            bytes_transferred += remain;
        }
        else
        {
            // no mask, autofrag
            BOOST_ASSERT(impl_->wr_buf_size != 0);
            buffers_suffix<
                ConstBufferSequence> cb{buffers};
            for(;;)
            {
                auto const n = clamp(remain, impl_->wr_buf_size);
                remain -= n;
                fh.len = n;
                fh.fin = fin ? remain == 0 : false;
                detail::fh_buffer fh_buf;
                detail::write<
                    flat_static_buffer_base>(fh_buf, fh);
                impl_->wr_cont = ! fin;
                net::write(impl_->stream,
                    beast::buffers_cat(fh_buf.data(),
                        beast::buffers_prefix(n, cb)), ec);
                if(! impl_->check_ok(ec))
                    return bytes_transferred;
                bytes_transferred += n;
                if(remain == 0)
                    break;
                fh.op = detail::opcode::cont;
                cb.consume(n);
            }
        }
    }
    else if(! impl_->wr_frag)
    {
        // mask, no autofrag
        fh.fin = fin;
        fh.len = remain;
        fh.key = this->create_mask();
        detail::prepared_key key;
        detail::prepare_key(key, fh.key);
        detail::fh_buffer fh_buf;
        detail::write<
            flat_static_buffer_base>(fh_buf, fh);
        buffers_suffix<
            ConstBufferSequence> cb{buffers};
        {
            auto const n =
                clamp(remain, impl_->wr_buf_size);
            auto const b =
                net::buffer(impl_->wr_buf.get(), n);
            net::buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            impl_->wr_cont = ! fin;
            net::write(impl_->stream,
                buffers_cat(fh_buf.data(), b), ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
        }
        while(remain > 0)
        {
            auto const n =
                clamp(remain, impl_->wr_buf_size);
            auto const b =
                net::buffer(impl_->wr_buf.get(), n);
            net::buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            net::write(impl_->stream, b, ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
        }
    }
    else
    {
        // mask, autofrag
        BOOST_ASSERT(impl_->wr_buf_size != 0);
        buffers_suffix<
            ConstBufferSequence> cb(buffers);
        for(;;)
        {
            fh.key = this->create_mask();
            detail::prepared_key key;
            detail::prepare_key(key, fh.key);
            auto const n =
                clamp(remain, impl_->wr_buf_size);
            auto const b =
                net::buffer(impl_->wr_buf.get(), n);
            net::buffer_copy(b, cb);
            detail::mask_inplace(b, key);
            fh.len = n;
            remain -= n;
            fh.fin = fin ? remain == 0 : false;
            impl_->wr_cont = ! fh.fin;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            net::write(impl_->stream,
                buffers_cat(fh_buf.data(), b), ec);
            if(! impl_->check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
            if(remain == 0)
                break;
            fh.op = detail::opcode::cont;
            cb.consume(n);
        }
    }
    return bytes_transferred;
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream<NextLayer, deflateSupported>::
async_write_some(bool fin,
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    write_some_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>{
            std::move(init.completion_handler), *this, fin, bs}(
                {}, 0, false);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
write(ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    auto const bytes_transferred = write(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer, deflateSupported>::
write(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    return write_some(true, buffers, ec);
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream<NextLayer, deflateSupported>::
async_write(
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    write_some_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>{
            std::move(init.completion_handler), *this, true, bs}(
                {}, 0, false);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
