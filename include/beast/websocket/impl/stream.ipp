//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_STREAM_IPP
#define BEAST_WEBSOCKET_IMPL_STREAM_IPP

#include <beast/websocket/teardown.hpp>
#include <beast/websocket/detail/hybi13.hpp>
#include <beast/websocket/impl/accept_op.ipp>
#include <beast/websocket/impl/close_op.ipp>
#include <beast/websocket/impl/handshake_op.ipp>
#include <beast/websocket/impl/ping_op.ipp>
#include <beast/websocket/impl/read_op.ipp>
#include <beast/websocket/impl/read_frame_op.ipp>
#include <beast/websocket/impl/response_op.ipp>
#include <beast/websocket/impl/write_op.ipp>
#include <beast/websocket/impl/write_frame_op.ipp>
#include <beast/http/read.hpp>
#include <beast/http/write.hpp>
#include <beast/http/reason.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/core/buffer_cat.hpp>
#include <beast/core/buffer_concepts.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/prepare_buffers.hpp>
#include <beast/core/static_streambuf.hpp>
#include <beast/core/stream_concepts.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <algorithm>
#include <memory>
#include <utility>

namespace beast {
namespace websocket {

template<class NextLayer>
template<class... Args>
stream<NextLayer>::
stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
}

template<class NextLayer>
void
stream<NextLayer>::
accept()
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(boost::asio::null_buffers{}, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
void
stream<NextLayer>::
accept(error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    accept(boost::asio::null_buffers{}, ec);
}

template<class NextLayer>
template<class AcceptHandler>
typename async_completion<
    AcceptHandler, void(error_code)>::result_type
stream<NextLayer>::
async_accept(AcceptHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    return async_accept(boost::asio::null_buffers{},
        std::forward<AcceptHandler>(handler));
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
accept(ConstBufferSequence const& buffers)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    accept(buffers, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
accept(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    reset();
    stream_.buffer().commit(buffer_copy(
        stream_.buffer().prepare(
            buffer_size(buffers)), buffers));
    http::request<http::string_body> m;
    http::read(next_layer(), stream_.buffer(), m, ec);
    if(ec)
        return;
    accept(m, ec);
}

template<class NextLayer>
template<class ConstBufferSequence, class AcceptHandler>
typename async_completion<
    AcceptHandler, void(error_code)>::result_type
stream<NextLayer>::
async_accept(ConstBufferSequence const& bs, AcceptHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    beast::async_completion<
        AcceptHandler, void(error_code)
            > completion(handler);
    accept_op<decltype(completion.handler)>{
        completion.handler, *this, bs};
    return completion.result.get();
}

template<class NextLayer>
template<class Body, class Headers>
void
stream<NextLayer>::
accept(http::request<Body, Headers> const& request)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(request, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
template<class Body, class Headers>
void
stream<NextLayer>::
accept(http::request<Body, Headers> const& req,
    error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    reset();
    auto const res = build_response(req);
    http::write(stream_, res, ec);
    if(ec)
        return;
    if(res.status != 101)
    {
        ec = error::handshake_failed;
        // VFALCO TODO Respect keep alive setting, perform
        //             teardown if Connection: close.
        return;
    }
    open(detail::role_type::server);
}

template<class NextLayer>
template<class Body, class Headers, class AcceptHandler>
typename async_completion<
    AcceptHandler, void(error_code)>::result_type
stream<NextLayer>::
async_accept(http::request<Body, Headers> const& req,
    AcceptHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    beast::async_completion<
        AcceptHandler, void(error_code)
            > completion(handler);
    reset();
    response_op<decltype(completion.handler)>{
        completion.handler, *this, req,
            boost_asio_handler_cont_helpers::
                is_continuation(completion.handler)};
    return completion.result.get();
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(boost::string_ref const& host,
    boost::string_ref const& resource)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    handshake(host, resource, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(boost::string_ref const& host,
    boost::string_ref const& resource, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    reset();
    std::string key;
    http::write(stream_,
        build_request(host, resource, key), ec);
    if(ec)
        return;
    http::response<http::string_body> res;
    http::read(next_layer(), stream_.buffer(), res, ec);
    if(ec)
        return;
    do_response(res, key, ec);
}

template<class NextLayer>
template<class HandshakeHandler>
typename async_completion<
    HandshakeHandler, void(error_code)>::result_type
stream<NextLayer>::
async_handshake(boost::string_ref const& host,
    boost::string_ref const& resource, HandshakeHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements not met");
    beast::async_completion<
        HandshakeHandler, void(error_code)
            > completion(handler);
    handshake_op<decltype(completion.handler)>{
        completion.handler, *this, host, resource};
    return completion.result.get();
}

template<class NextLayer>
void
stream<NextLayer>::
close(close_reason const& cr)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    close(cr, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
void
stream<NextLayer>::
close(close_reason const& cr, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    BOOST_ASSERT(! wr_close_);
    wr_close_ = true;
    detail::frame_streambuf fb;
    write_close<static_streambuf>(fb, cr);
    boost::asio::write(stream_, fb.data(), ec);
    failed_ = ec != 0;
}

template<class NextLayer>
template<class CloseHandler>
typename async_completion<
    CloseHandler, void(error_code)>::result_type
stream<NextLayer>::
async_close(close_reason const& cr, CloseHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements not met");
    beast::async_completion<
        CloseHandler, void(error_code)
            > completion(handler);
    close_op<decltype(completion.handler)>{
        completion.handler, *this, cr};
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
template<class DynamicBuffer>
void
stream<NextLayer>::
read(opcode& op, DynamicBuffer& dynabuf)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    read(op, dynabuf, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
template<class DynamicBuffer>
void
stream<NextLayer>::
read(opcode& op, DynamicBuffer& dynabuf, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    frame_info fi;
    for(;;)
    {
        read_frame(fi, dynabuf, ec);
        if(ec)
            break;
        op = fi.op;
        if(fi.fin)
            break;
    }
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code)>::result_type
stream<NextLayer>::
async_read(opcode& op,
    DynamicBuffer& dynabuf, ReadHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    beast::async_completion<
        ReadHandler, void(error_code)
            > completion(handler);
    read_op<DynamicBuffer, decltype(completion.handler)>{
        completion.handler, *this, op, dynabuf};
    return completion.result.get();
}

template<class NextLayer>
template<class DynamicBuffer>
void
stream<NextLayer>::
read_frame(frame_info& fi, DynamicBuffer& dynabuf)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    read_frame(fi, dynabuf, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
template<class DynamicBuffer>
void
stream<NextLayer>::
read_frame(frame_info& fi, DynamicBuffer& dynabuf, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    close_code::value code{};
    for(;;)
    {
        if(rd_need_ == 0)
        {
            // read header
            detail::frame_streambuf fb;
            do_read_fh(fb, code, ec);
            failed_ = ec != 0;
            if(failed_)
                return;
            if(code != close_code::none)
                break;
            if(detail::is_control(rd_fh_.op))
            {
                // read control payload
                if(rd_fh_.len > 0)
                {
                    auto const mb = fb.prepare(
                        static_cast<std::size_t>(rd_fh_.len));
                    fb.commit(boost::asio::read(stream_, mb, ec));
                    failed_ = ec != 0;
                    if(failed_)
                        return;
                    if(rd_fh_.mask)
                        detail::mask_inplace(mb, rd_key_);
                    fb.commit(static_cast<std::size_t>(rd_fh_.len));
                }
                if(rd_fh_.op == opcode::ping)
                {
                    ping_data data;
                    detail::read(data, fb.data());
                    fb.reset();
                    write_ping<static_streambuf>(
                        fb, opcode::pong, data);
                    boost::asio::write(stream_, fb.data(), ec);
                    failed_ = ec != 0;
                    if(failed_)
                        return;
                    continue;
                }
                else if(rd_fh_.op == opcode::pong)
                {
                    ping_data payload;
                    detail::read(payload, fb.data());
                    if(pong_cb_)
                        pong_cb_(payload);
                    continue;
                }
                BOOST_ASSERT(rd_fh_.op == opcode::close);
                {
                    detail::read(cr_, fb.data(), code);
                    if(code != close_code::none)
                        break;
                    if(! wr_close_)
                    {
                        auto cr = cr_;
                        if(cr.code == close_code::none)
                            cr.code = close_code::normal;
                        cr.reason = "";
                        fb.reset();
                        wr_close_ = true;
                        write_close<static_streambuf>(fb, cr);
                        boost::asio::write(stream_, fb.data(), ec);
                        failed_ = ec != 0;
                        if(failed_)
                            return;
                    }
                    break;
                }
            }
            if(rd_need_ == 0 && ! rd_fh_.fin)
            {
                // empty frame
                continue;
            }
        }
        // read payload
        auto smb = dynabuf.prepare(
            detail::clamp(rd_need_));
        auto const bytes_transferred =
            stream_.read_some(smb, ec);
        failed_ = ec != 0;
        if(failed_)
            return;
        rd_need_ -= bytes_transferred;
        auto const pb = prepare_buffers(
            bytes_transferred, smb);
        if(rd_fh_.mask)
            detail::mask_inplace(pb, rd_key_);
        if(rd_opcode_ == opcode::text)
        {
            if(! rd_utf8_check_.write(pb) ||
                (rd_need_ == 0 && rd_fh_.fin &&
                    ! rd_utf8_check_.finish()))
            {
                code = close_code::bad_payload;
                break;
            }
        }
        dynabuf.commit(bytes_transferred);
        fi.op = rd_opcode_;
        fi.fin = rd_fh_.fin && rd_need_ == 0;
        return;
    }
    if(code != close_code::none)
    {
        // Fail the connection (per rfc6455)
        if(! wr_close_)
        {
            wr_close_ = true;
            detail::frame_streambuf fb;
            write_close<static_streambuf>(fb, code);
            boost::asio::write(stream_, fb.data(), ec);
            failed_ = ec != 0;
            if(failed_)
                return;
        }
        websocket_helpers::call_teardown(next_layer(), ec);
        failed_ = ec != 0;
        if(failed_)
            return;
        ec = error::failed;
        failed_ = true;
        return;
    }
    if(! ec)
        websocket_helpers::call_teardown(next_layer(), ec);
    if(! ec)
        ec = error::closed;
    failed_ = ec != 0;
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code)>::result_type
stream<NextLayer>::
async_read_frame(frame_info& fi,
    DynamicBuffer& dynabuf, ReadHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(beast::is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    beast::async_completion<
        ReadHandler, void(error_code)> completion(handler);
    read_frame_op<DynamicBuffer, decltype(completion.handler)>{
        completion.handler, *this, fi, dynabuf};
    return completion.result.get();
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write(ConstBufferSequence const& buffers)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    write(buffers, ec);
    if(ec)
        throw system_error{ec};
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    write_frame(true, buffers, ec);
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
typename async_completion<
    WriteHandler, void(error_code)>::result_type
stream<NextLayer>::
async_write(ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    beast::async_completion<
        WriteHandler, void(error_code)> completion(handler);
    write_op<ConstBufferSequence, decltype(completion.handler)>{
        completion.handler, *this, bs};
    return completion.result.get();
}

template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write_frame(bool fin, ConstBufferSequence const& buffers)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    write_frame(fin, buffers, ec);
    if(ec)
        throw system_error{ec};
}

/*
if(compress)
    compress buffers into write_buffer
    if(write_buffer_avail == write_buffer_size || fin`)
        if(mask)
            apply mask to write buffer
        write frame header, write_buffer as one frame
else if(auto-fragment)
    if(fin || write_buffer_avail + buffers size == write_buffer_size)
        if(mask)
            append buffers to write buffer
            apply mask to write buffer
            write frame header, write buffer as one frame

        else:
            write frame header, write buffer, and buffers as one frame
    else:
        append buffers to write buffer
else if(mask)
    copy buffers to write_buffer
    apply mask to write_buffer
    write frame header and possibly full write_buffer in a single call
    loop:
        copy buffers to write_buffer
        apply mask to write_buffer
        write write_buffer in a single call
else
        write frame header, buffers as one frame
*/
template<class NextLayer>
template<class ConstBufferSequence>
void
stream<NextLayer>::
write_frame(bool fin,
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_SyncStream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    bool const compress = false;
    if(! wr_.cont)
        wr_prepare(compress);
    detail::frame_header fh;
    fh.op = wr_.cont ? opcode::cont : wr_opcode_;
    fh.rsv1 = false;
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.mask = role_ == detail::role_type::client;
    auto remain = buffer_size(buffers);
    if(compress)
    {
        // TODO
    }
    else if(wr_.autofrag)
    {
        consuming_buffers<ConstBufferSequence> cb(buffers);
        do
        {
            auto const room = wr_.max - wr_.size;
            if(! fin && remain < room)
            {
                buffer_copy(
                    buffer(wr_.buf.get() + wr_.size, remain), cb);
                wr_.size += remain;
                return;
            }
            auto const n = detail::clamp(remain, room);
            buffer_copy(
                buffer(wr_.buf.get() + wr_.size, n), cb);
            auto const mb = buffer(wr_.buf.get(), wr_.size + n);
            if(fh.mask)
            {
                fh.key = maskgen_();
                detail::prepared_key_type key;
                detail::prepare_key(key, fh.key);
                detail::mask_inplace(mb, key);
            }
            fh.fin = fin && n == remain;
            fh.len = buffer_size(mb);
            detail::fh_streambuf fh_buf;
            detail::write<static_streambuf>(fh_buf, fh);
            // send header and payload
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), mb), ec);
            failed_ = ec != 0;
            if(failed_)
                return;
            remain -= n;
            cb.consume(n);
            wr_.size = 0;
            fh.op = opcode::cont;
        }
        while(remain > 0);
        wr_.cont = ! fh.fin;
        return;
    }
    else if(fh.mask)
    {
        consuming_buffers<ConstBufferSequence> cb(buffers);
        fh.fin = fin;
        fh.len = remain;
        fh.key = maskgen_();
        wr_.cont = ! fh.fin;
        detail::fh_streambuf fh_buf;
        detail::write<static_streambuf>(fh_buf, fh);
        detail::prepared_key_type key;
        detail::prepare_key(key, fh.key);
        {
            auto const n = detail::clamp(remain, wr_.max);
            auto const mb = buffer(wr_.buf.get(), n);
            buffer_copy(mb, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(mb, key);
            // send header and payload
            boost::asio::write(stream_,
                buffer_cat(fh_buf.data(), mb), ec);
            failed_ = ec != 0;
            if(failed_)
                return;
        }
        while(remain > 0)
        {
            auto const n = detail::clamp(remain, wr_.max);
            auto const mb = buffer(wr_.buf.get(), n);
            buffer_copy(mb, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(mb, key);
            // send payload
            boost::asio::write(stream_, mb, ec);
            failed_ = ec != 0;
            if(failed_)
                return;
        }
        return;
    }
    {
        // send header and payload
        fh.fin = fin;
        fh.len = remain;
        wr_.cont = ! fh.fin;
        detail::fh_streambuf fh_buf;
        detail::write<static_streambuf>(fh_buf, fh);
        boost::asio::write(stream_,
            buffer_cat(fh_buf.data(), buffers), ec);
        failed_ = ec != 0;
    }
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
typename async_completion<
    WriteHandler, void(error_code)>::result_type
stream<NextLayer>::
async_write_frame(bool fin,
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_AsyncStream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(beast::is_ConstBufferSequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    beast::async_completion<
        WriteHandler, void(error_code)
            > completion(handler);
    write_frame_op<ConstBufferSequence, decltype(
        completion.handler)>{completion.handler,
            *this, fin, bs};
    return completion.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
void
stream<NextLayer>::
reset()
{
    failed_ = false;
    rd_need_ = 0;
    rd_cont_ = false;
    wr_close_ = false;
    wr_.cont = false;
    wr_block_ = nullptr;    // should be nullptr on close anyway
    pong_data_ = nullptr;   // should be nullptr on close anyway

    stream_.buffer().consume(
        stream_.buffer().size());
}

template<class NextLayer>
http::request<http::empty_body>
stream<NextLayer>::
build_request(boost::string_ref const& host,
    boost::string_ref const& resource, std::string& key)
{
    http::request<http::empty_body> req;
    req.url = { resource.data(), resource.size() };
    req.version = 11;
    req.method = "GET";
    req.headers.insert("Host", host);
    req.headers.insert("Upgrade", "websocket");
    key = detail::make_sec_ws_key(maskgen_);
    req.headers.insert("Sec-WebSocket-Key", key);
    req.headers.insert("Sec-WebSocket-Version", "13");
    (*d_)(req);
    http::prepare(req, http::connection::upgrade);
    return req;
}

template<class NextLayer>
template<class Body, class Headers>
http::response<http::string_body>
stream<NextLayer>::
build_response(http::request<Body, Headers> const& req)
{
    auto err =
        [&](std::string const& text)
        {
            http::response<http::string_body> res;
            res.status = 400;
            res.reason = http::reason_string(res.status);
            res.version = req.version;
            res.body = text;
            (*d_)(res);
            prepare(res,
                (is_keep_alive(req) && keep_alive_) ?
                    http::connection::keep_alive :
                    http::connection::close);
            return res;
        };
    if(req.version < 11)
        return err("HTTP version 1.1 required");
    if(req.method != "GET")
        return err("Wrong method");
    if(! is_upgrade(req))
        return err("Expected Upgrade request");
    if(! req.headers.exists("Host"))
        return err("Missing Host");
    if(! req.headers.exists("Sec-WebSocket-Key"))
        return err("Missing Sec-WebSocket-Key");
    if(! http::token_list{req.headers["Upgrade"]}.exists("websocket"))
        return err("Missing websocket Upgrade token");
    {
        auto const version =
            req.headers["Sec-WebSocket-Version"];
        if(version.empty())
            return err("Missing Sec-WebSocket-Version");
        if(version != "13")
        {
            http::response<http::string_body> res;
            res.status = 426;
            res.reason = http::reason_string(res.status);
            res.version = req.version;
            res.headers.insert("Sec-WebSocket-Version", "13");
            prepare(res,
                (is_keep_alive(req) && keep_alive_) ?
                    http::connection::keep_alive :
                    http::connection::close);
            return res;
        }
    }
    http::response<http::string_body> res;
    res.status = 101;
    res.reason = http::reason_string(res.status);
    res.version = req.version;
    res.headers.insert("Upgrade", "websocket");
    {
        auto const key =
            req.headers["Sec-WebSocket-Key"];
        res.headers.insert("Sec-WebSocket-Accept",
            detail::make_sec_ws_accept(key));
    }
    res.headers.replace("Server", "Beast.WSProto");
    (*d_)(res);
    http::prepare(res, http::connection::upgrade);
    return res;
}

template<class NextLayer>
template<class Body, class Headers>
void
stream<NextLayer>::
do_response(http::response<Body, Headers> const& res,
    boost::string_ref const& key, error_code& ec)
{
    // VFALCO Review these error codes
    auto fail = [&]{ ec = error::response_failed; };
    if(res.version < 11)
        return fail();
    if(res.status != 101)
        return fail();
    if(! is_upgrade(res))
        return fail();
    if(! http::token_list{res.headers["Upgrade"]}.exists("websocket"))
        return fail();
    if(! res.headers.exists("Sec-WebSocket-Accept"))
        return fail();
    if(res.headers["Sec-WebSocket-Accept"] !=
        detail::make_sec_ws_accept(key))
        return fail();
    open(detail::role_type::client);
}

template<class NextLayer>
void
stream<NextLayer>::
do_read_fh(detail::frame_streambuf& fb,
    close_code::value& code, error_code& ec)
{
    fb.commit(boost::asio::read(
        stream_, fb.prepare(2), ec));
    if(ec)
        return;
    auto const n = read_fh1(fb, code);
    if(code != close_code::none)
        return;
    if(n > 0)
    {
        fb.commit(boost::asio::read(
            stream_, fb.prepare(n), ec));
        if(ec)
            return;
    }
    read_fh2(fb, code);
}

} // websocket
} // beast

#endif
