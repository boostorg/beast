//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_WRITE_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_WRITE_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_cat.hpp>
#include <boost/beast/core/buffer_prefix.hpp>
#include <boost/beast/core/consuming_buffers.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

template<class NextLayer>
template<class Buffers, class Handler>
class stream<NextLayer>::write_some_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    consuming_buffers<Buffers> cb_;
    detail::frame_header fh_;
    detail::prepared_key key_;
    std::size_t remain_;
    token tok_;
    int how_;
    bool fin_;
    bool more_;

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = default;

    template<class DeducedHandler>
    write_some_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        bool fin,
        Buffers const& bs)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , cb_(bs)
        , tok_(ws_.t_.unique())
        , fin_(fin)
    {
    }

    Handler&
    handler()
    {
        return h_;
    }

    void operator()(
        error_code ec,
        std::size_t bytes_transferred,
        bool)
    {
        (*this)(ec, bytes_transferred);
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, write_some_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, write_some_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(write_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class Buffers, class Handler>
void
stream<NextLayer>::
write_some_op<Buffers, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred)
{
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using boost::asio::mutable_buffers_1;
    enum
    {
        do_nomask_nofrag,
        do_nomask_frag,
        do_mask_nofrag,
        do_mask_frag,
        do_deflate
    };
    std::size_t n;
    boost::asio::mutable_buffer b;

    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Set up the outgoing frame header
        if(! ws_.wr_.cont)
        {
            ws_.wr_begin();
            fh_.rsv1 = ws_.wr_.compress;
        }
        else
        {
            fh_.rsv1 = false;
        }
        fh_.rsv2 = false;
        fh_.rsv3 = false;
        fh_.op = ws_.wr_.cont ?
            detail::opcode::cont : ws_.wr_opcode_;
        fh_.mask =
            ws_.role_ == role_type::client;

        // Choose a write algorithm
        if(ws_.wr_.compress)
        {
            how_ = do_deflate;
        }
        else if(! fh_.mask)
        {
            if(! ws_.wr_.autofrag)
            {
                how_ = do_nomask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.wr_.buf_size != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.wr_.buf_size)
                    how_ = do_nomask_frag;
                else
                    how_ = do_nomask_nofrag;
            }
        }
        else
        {
            if(! ws_.wr_.autofrag)
            {
                how_ = do_mask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.wr_.buf_size != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.wr_.buf_size)
                    how_ = do_mask_frag;
                else
                    how_ = do_mask_nofrag;
            }
        }

    do_maybe_suspend:
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
            ws_.wr_op_.save(std::move(*this));

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

        //------------------------------------------------------------------

        if(how_ == do_nomask_nofrag)
        {
            fh_.fin = fin_;
            fh_.len = buffer_size(cb_);
            ws_.wr_.fb.reset();
            detail::write<flat_static_buffer_base>(
                ws_.wr_.fb, fh_);
            ws_.wr_.cont = ! fin_;
            // Send frame
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(ws_.stream_,
                buffer_cat(ws_.wr_.fb.data(), cb_),
                    std::move(*this));
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            if(ec)
                ws_.failed_ = true;
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_nomask_frag)
        {
            for(;;)
            {
                fh_.len = clamp(remain_, ws_.wr_.buf_size);
                remain_ -= clamp(fh_.len);
                fh_.fin = fin_ ? remain_ == 0 : false;
                ws_.wr_.fb.reset();
                detail::write<flat_static_buffer_base>(
                    ws_.wr_.fb, fh_);
                ws_.wr_.cont = ! fin_;
                // Send frame
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(
                    ws_.stream_, buffer_cat(
                        ws_.wr_.fb.data(), buffer_prefix(
                            clamp(fh_.len), cb_)),
                                std::move(*this));
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                if(ec)
                {
                    ws_.failed_ = true;
                    goto upcall;
                }
                if(remain_ == 0)
                    goto upcall;
                cb_.consume(
                    bytes_transferred - ws_.wr_.fb.size());
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames
                ws_.wr_block_.reset();
                if( ws_.close_op_.maybe_invoke() ||
                    ws_.rd_op_.maybe_invoke() ||
                    ws_.ping_op_.maybe_invoke())
                {
                    BOOST_ASIO_CORO_YIELD
                    ws_.get_io_service().post(
                        std::move(*this));
                    goto do_maybe_suspend;
                }
                ws_.wr_block_ = tok_;
            }
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_nofrag)
        {
            remain_ = buffer_size(cb_);
            fh_.fin = fin_;
            fh_.len = remain_;
            fh_.key = ws_.maskgen_();
            detail::prepare_key(key_, fh_.key);
            ws_.wr_.fb.reset();
            detail::write<flat_static_buffer_base>(
                ws_.wr_.fb, fh_);
            n = clamp(remain_, ws_.wr_.buf_size);
            buffer_copy(buffer(
                ws_.wr_.buf.get(), n), cb_);
            detail::mask_inplace(buffer(
                ws_.wr_.buf.get(), n), key_);
            remain_ -= n;
            ws_.wr_.cont = ! fin_;
            // Send frame header and partial payload
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(
                ws_.stream_, buffer_cat(ws_.wr_.fb.data(),
                    buffer(ws_.wr_.buf.get(), n)),
                        std::move(*this));
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            if(ec)
            {
                ws_.failed_ = true;
                goto upcall;
            }
            while(remain_ > 0)
            {
                cb_.consume(ws_.wr_.buf_size);
                n = clamp(remain_, ws_.wr_.buf_size);
                buffer_copy(buffer(
                    ws_.wr_.buf.get(), n), cb_);
                detail::mask_inplace(buffer(
                    ws_.wr_.buf.get(), n), key_);
                remain_ -= n;
                // Send partial payload
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffer(ws_.wr_.buf.get(), n),
                        std::move(*this));
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                if(ec)
                {
                    ws_.failed_ = true;
                    goto upcall;
                }
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_frag)
        {
            for(;;)
            {
                n = clamp(remain_, ws_.wr_.buf_size);
                remain_ -= n;
                fh_.len = n;
                fh_.key = ws_.maskgen_();
                fh_.fin = fin_ ? remain_ == 0 : false;
                detail::prepare_key(key_, fh_.key);
                buffer_copy(buffer(
                    ws_.wr_.buf.get(), n), cb_);
                detail::mask_inplace(buffer(
                    ws_.wr_.buf.get(), n), key_);
                ws_.wr_.fb.reset();
                detail::write<flat_static_buffer_base>(
                    ws_.wr_.fb, fh_);
                ws_.wr_.cont = ! fin_;
                // Send frame
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffer_cat(ws_.wr_.fb.data(),
                        buffer(ws_.wr_.buf.get(), n)),
                            std::move(*this));
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                if(ec)
                {
                    ws_.failed_ = true;
                    goto upcall;
                }
                if(remain_ == 0)
                    goto upcall;
                cb_.consume(
                    bytes_transferred - ws_.wr_.fb.size());
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames:
                ws_.wr_block_.reset();
                if( ws_.close_op_.maybe_invoke() ||
                    ws_.rd_op_.maybe_invoke() ||
                    ws_.ping_op_.maybe_invoke())
                {
                    BOOST_ASIO_CORO_YIELD
                    ws_.get_io_service().post(
                        std::move(*this));
                    goto do_maybe_suspend;
                }
                ws_.wr_block_ = tok_;
            }
        }

        //------------------------------------------------------------------

        else if(how_ == do_deflate)
        {
            for(;;)
            {
                b = buffer(ws_.wr_.buf.get(),
                    ws_.wr_.buf_size);
                more_ = detail::deflate(
                    ws_.pmd_->zo, b, cb_, fin_, ec);
                ws_.failed_ = !!ec;
                if(ws_.failed_)
                {
                    // Always dispatching is easiest
                    BOOST_ASIO_CORO_YIELD
                    ws_.get_io_service().post(
                        bind_handler(std::move(*this), ec));
                    goto upcall;
                }
                n = buffer_size(b);
                if(n == 0)
                {
                    // The input was consumed, but there
                    // is no output due to compression
                    // latency.
                    BOOST_ASSERT(! fin_);
                    BOOST_ASSERT(buffer_size(cb_) == 0);

                    // We can skip the dispatch if the
                    // asynchronous initiation function is
                    // not on call stack but its hard to
                    // figure out so be safe and dispatch.
                    BOOST_ASIO_CORO_YIELD
                    ws_.get_io_service().post(
                        std::move(*this));
                    goto upcall;
                }
                if(fh_.mask)
                {
                    fh_.key = ws_.maskgen_();
                    detail::prepared_key key;
                    detail::prepare_key(key, fh_.key);
                    detail::mask_inplace(b, key);
                }
                fh_.fin = ! more_;
                fh_.len = n;
                ws_.wr_.fb.reset();
                detail::write<
                    flat_static_buffer_base>(ws_.wr_.fb, fh_);
                ws_.wr_.cont = ! fin_;
                // Send frame
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffer_cat(ws_.wr_.fb.data(),
                        mutable_buffers_1{b}), std::move(*this));
                BOOST_ASSERT(ws_.wr_block_ == tok_);
                if(ec)
                {
                    ws_.failed_ = true;
                    goto upcall;
                }
                if(more_)
                {
                    fh_.op = detail::opcode::cont;
                    fh_.rsv1 = false;
                    // Allow outgoing control frames to
                    // be sent in between message frames:
                    ws_.wr_block_.reset();
                    if( ws_.close_op_.maybe_invoke() ||
                        ws_.rd_op_.maybe_invoke() ||
                        ws_.ping_op_.maybe_invoke())
                    {
                        BOOST_ASIO_CORO_YIELD
                        ws_.get_io_service().post(
                            std::move(*this));
                        goto do_maybe_suspend;
                    }
                    ws_.wr_block_ = tok_;
                }
                else
                {
                    BOOST_ASSERT(ws_.wr_block_ == tok_);
                    if(fh_.fin && (
                        (ws_.role_ == role_type::client &&
                            ws_.pmd_config_.client_no_context_takeover) ||
                        (ws_.role_ == role_type::server &&
                            ws_.pmd_config_.server_no_context_takeover)))
                        ws_.pmd_->zo.reset();
                    goto upcall;
                }
            }
        }

    //--------------------------------------------------------------------------

    upcall:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        ws_.wr_block_.reset();
        ws_.close_op_.maybe_invoke() ||
            ws_.rd_op_.maybe_invoke() ||
            ws_.ping_op_.maybe_invoke();
        h_(ec);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write_some(bool fin, ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    write_some(fin, buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write_some(bool fin,
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    // Make sure the stream is open
    if(failed_)
    {
        ec = boost::asio::error::operation_aborted;
        return;
    }
    detail::frame_header fh;
    if(! wr_.cont)
    {
        wr_begin();
        fh.rsv1 = wr_.compress;
    }
    else
    {
        fh.rsv1 = false;
    }
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.op = wr_.cont ?
        detail::opcode::cont : wr_opcode_;
    fh.mask = role_ == role_type::client;
    auto remain = buffer_size(buffers);
    if(wr_.compress)
    {
        consuming_buffers<
            ConstBufferSequence> cb{buffers};
        for(;;)
        {
            auto b = buffer(
                wr_.buf.get(), wr_.buf_size);
            auto const more = detail::deflate(
                pmd_->zo, b, cb, fin, ec);
            failed_ = !!ec;
            if(failed_)
                return;
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
                fh.key = maskgen_();
                detail::prepared_key key;
                detail::prepare_key(key, fh.key);
                detail::mask_inplace(b, key);
            }
            fh.fin = ! more;
            fh.len = n;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            wr_.cont = ! fin;
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), b), ec);
            failed_ = !!ec;
            if(failed_)
                return;
            if(! more)
                break;
            fh.op = detail::opcode::cont;
            fh.rsv1 = false;
        }
        if(fh.fin && (
            (role_ == role_type::client &&
                pmd_config_.client_no_context_takeover) ||
            (role_ == role_type::server &&
                pmd_config_.server_no_context_takeover)))
            pmd_->zo.reset();
        return;
    }
    if(! fh.mask)
    {
        if(! wr_.autofrag)
        {
            // no mask, no autofrag
            fh.fin = fin;
            fh.len = remain;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            wr_.cont = ! fin;
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), buffers), ec);
            failed_ = !!ec;
            if(failed_)
                return;
        }
        else
        {
            // no mask, autofrag
            BOOST_ASSERT(wr_.buf_size != 0);
            consuming_buffers<
                ConstBufferSequence> cb{buffers};
            for(;;)
            {
                auto const n = clamp(remain, wr_.buf_size);
                remain -= n;
                fh.len = n;
                fh.fin = fin ? remain == 0 : false;
                detail::fh_buffer fh_buf;
                detail::write<
                    flat_static_buffer_base>(fh_buf, fh);
                wr_.cont = ! fin;
                boost::asio::write(stream_,
                    buffer_cat(fh_buf.data(),
                        buffer_prefix(n, cb)), ec);
                failed_ = !!ec;
                if(failed_)
                    return;
                if(remain == 0)
                    break;
                fh.op = detail::opcode::cont;
                cb.consume(n);
            }
        }
        return;
    }
    if(! wr_.autofrag)
    {
        // mask, no autofrag
        fh.fin = fin;
        fh.len = remain;
        fh.key = maskgen_();
        detail::prepared_key key;
        detail::prepare_key(key, fh.key);
        detail::fh_buffer fh_buf;
        detail::write<
            flat_static_buffer_base>(fh_buf, fh);
        consuming_buffers<
            ConstBufferSequence> cb{buffers};
        {
            auto const n = clamp(remain, wr_.buf_size);
            auto const b = buffer(wr_.buf.get(), n);
            buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            wr_.cont = ! fin;
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), b), ec);
            failed_ = !!ec;
            if(failed_)
                return;
        }
        while(remain > 0)
        {
            auto const n = clamp(remain, wr_.buf_size);
            auto const b = buffer(wr_.buf.get(), n);
            buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            boost::asio::write(stream_, b, ec);
            failed_ = !!ec;
            if(failed_)
                return;
        }
        return;
    }
    {
        // mask, autofrag
        BOOST_ASSERT(wr_.buf_size != 0);
        consuming_buffers<
            ConstBufferSequence> cb{buffers};
        for(;;)
        {
            fh.key = maskgen_();
            detail::prepared_key key;
            detail::prepare_key(key, fh.key);
            auto const n = clamp(remain, wr_.buf_size);
            auto const b = buffer(wr_.buf.get(), n);
            buffer_copy(b, cb);
            detail::mask_inplace(b, key);
            fh.len = n;
            remain -= n;
            fh.fin = fin ? remain == 0 : false;
            wr_.cont = ! fh.fin;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), b), ec);
            failed_ = !!ec;
            if(failed_)
                return;
            if(remain == 0)
                break;
            fh.op = detail::opcode::cont;
            cb.consume(n);
        }
        return;
    }
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
stream<NextLayer>::
async_write_some(bool fin,
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    write_some_op<ConstBufferSequence, handler_type<
        WriteHandler, void(error_code)>>{init.completion_handler,
            *this, fin, bs}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write(ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    write(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    write_some(true, buffers, ec);
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
stream<NextLayer>::
async_write(
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(beast::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    write_some_op<ConstBufferSequence, handler_type<
        WriteHandler, void(error_code)>>{init.completion_handler,
            *this, true, bs}();
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
