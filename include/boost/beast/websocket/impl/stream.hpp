//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_STREAM_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_STREAM_HPP

#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/make_unique.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {

template<class NextLayer, bool deflateSupported>
template<class... Args>
stream<NextLayer, deflateSupported>::
stream(Args&&... args)
    : impl_(std::make_shared<impl_type>(
        std::forward<Args>(args)...))
{
    BOOST_ASSERT(impl_->rd_buf.max_size() >=
        max_control_frame_size);
}

template<class NextLayer, bool deflateSupported>
auto
stream<NextLayer, deflateSupported>::
get_executor() const noexcept ->
    executor_type
{
    return impl_->stream.get_executor();
}

template<class NextLayer, bool deflateSupported>
auto
stream<NextLayer, deflateSupported>::
next_layer() noexcept ->
    next_layer_type&
{
    return impl_->stream;
}

template<class NextLayer, bool deflateSupported>
auto
stream<NextLayer, deflateSupported>::
next_layer() const noexcept ->
    next_layer_type const&
{
    return impl_->stream;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
is_open() const noexcept
{
    return impl_->status_ == status::open;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
got_binary() const noexcept
{
    return impl_->rd_op == detail::opcode::binary;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
is_message_done() const noexcept
{
    return impl_->rd_done;
}

template<class NextLayer, bool deflateSupported>
close_reason const&
stream<NextLayer, deflateSupported>::
reason() const noexcept
{
    return impl_->cr;
}

template<class NextLayer, bool deflateSupported>
std::size_t
stream<NextLayer, deflateSupported>::
read_size_hint(
    std::size_t initial_size) const
{
    return impl_->read_size_hint_pmd(
        initial_size, impl_->rd_done,
        impl_->rd_remain, impl_->rd_fh);
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer, class>
std::size_t
stream<NextLayer, deflateSupported>::
read_size_hint(DynamicBuffer& buffer) const
{
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    auto const initial_size = (std::min)(
        +tcp_frame_size,
        buffer.max_size() - buffer.size());
    if(initial_size == 0)
        return 1; // buffer is full
    return read_size_hint(initial_size);
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
set_option(permessage_deflate const& o)
{
    impl_->set_option_pmd(o);
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
get_option(permessage_deflate& o)
{
    impl_->get_option_pmd(o);
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
auto_fragment(bool value)
{
    impl_->wr_frag_opt = value;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
auto_fragment() const
{
    return impl_->wr_frag_opt;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
binary(bool value)
{
    impl_->wr_opcode = value ?
        detail::opcode::binary :
        detail::opcode::text;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
binary() const
{
    return impl_->wr_opcode == detail::opcode::binary;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
control_callback(std::function<
    void(frame_type, string_view)> cb)
{
    impl_->ctrl_cb = std::move(cb);
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
control_callback()
{
    impl_->ctrl_cb = {};
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
read_message_max(std::size_t amount)
{
    impl_->rd_msg_max = amount;
}

template<class NextLayer, bool deflateSupported>
std::size_t
stream<NextLayer, deflateSupported>::
read_message_max() const
{
    return impl_->rd_msg_max;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
secure_prng(bool value)
{
    this->secure_prng_ = value;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
write_buffer_size(std::size_t amount)
{
    if(amount < 8)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "write buffer size underflow"});
    impl_->wr_buf_opt = amount;
}

template<class NextLayer, bool deflateSupported>
std::size_t
stream<NextLayer, deflateSupported>::
write_buffer_size() const
{
    return impl_->wr_buf_opt;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
text(bool value)
{
    impl_->wr_opcode = value ?
        detail::opcode::text :
        detail::opcode::binary;
}

template<class NextLayer, bool deflateSupported>
bool
stream<NextLayer, deflateSupported>::
text() const
{
    return impl_->wr_opcode == detail::opcode::text;
}

//------------------------------------------------------------------------------

// Attempt to read a complete frame header.
// Returns `false` if more bytes are needed
template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
bool
stream<NextLayer, deflateSupported>::
parse_fh(
    detail::frame_header& fh,
    DynamicBuffer& b,
    error_code& ec)
{
    if(buffer_size(b.data()) < 2)
    {
        // need more bytes
        ec = {};
        return false;
    }
    buffers_suffix<typename
        DynamicBuffer::const_buffers_type> cb{
            b.data()};
    std::size_t need;
    {
        std::uint8_t tmp[2];
        cb.consume(net::buffer_copy(
            net::buffer(tmp), cb));
        fh.len = tmp[1] & 0x7f;
        switch(fh.len)
        {
            case 126: need = 2; break;
            case 127: need = 8; break;
            default:
                need = 0;
        }
        fh.mask = (tmp[1] & 0x80) != 0;
        if(fh.mask)
            need += 4;
        if(buffer_size(cb) < need)
        {
            // need more bytes
            ec = {};
            return false;
        }
        fh.op   = static_cast<
            detail::opcode>(tmp[0] & 0x0f);
        fh.fin  = (tmp[0] & 0x80) != 0;
        fh.rsv1 = (tmp[0] & 0x40) != 0;
        fh.rsv2 = (tmp[0] & 0x20) != 0;
        fh.rsv3 = (tmp[0] & 0x10) != 0;
    }
    switch(fh.op)
    {
    case detail::opcode::binary:
    case detail::opcode::text:
        if(impl_->rd_cont)
        {
            // new data frame when continuation expected
            ec = error::bad_data_frame;
            return false;
        }
        if(fh.rsv2 || fh.rsv3 ||
            ! impl_->rd_deflated(fh.rsv1))
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;

    case detail::opcode::cont:
        if(! impl_->rd_cont)
        {
            // continuation without an active message
            ec = error::bad_continuation;
            return false;
        }
        if(fh.rsv1 || fh.rsv2 || fh.rsv3)
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;

    default:
        if(detail::is_reserved(fh.op))
        {
            // reserved opcode
            ec = error::bad_opcode;
            return false;
        }
        if(! fh.fin)
        {
            // fragmented control message
            ec = error::bad_control_fragment;
            return false;
        }
        if(fh.len > 125)
        {
            // invalid length for control message
            ec = error::bad_control_size;
            return false;
        }
        if(fh.rsv1 || fh.rsv2 || fh.rsv3)
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;
    }
    if(impl_->role == role_type::server && ! fh.mask)
    {
        // unmasked frame from client
        ec = error::bad_unmasked_frame;
        return false;
    }
    if(impl_->role == role_type::client && fh.mask)
    {
        // masked frame from server
        ec = error::bad_masked_frame;
        return false;
    }
    if(detail::is_control(fh.op) &&
        buffer_size(cb) < need + fh.len)
    {
        // Make the entire control frame payload
        // get read in before we return `true`
        return false;
    }
    switch(fh.len)
    {
    case 126:
    {
        std::uint8_t tmp[2];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(net::buffer_copy(net::buffer(tmp), cb));
        fh.len = detail::big_uint16_to_native(&tmp[0]);
        if(fh.len < 126)
        {
            // length not canonical
            ec = error::bad_size;
            return false;
        }
        break;
    }
    case 127:
    {
        std::uint8_t tmp[8];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(net::buffer_copy(net::buffer(tmp), cb));
        fh.len = detail::big_uint64_to_native(&tmp[0]);
        if(fh.len < 65536)
        {
            // length not canonical
            ec = error::bad_size;
            return false;
        }
        break;
    }
    }
    if(fh.mask)
    {
        std::uint8_t tmp[4];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(net::buffer_copy(net::buffer(tmp), cb));
        fh.key = detail::little_uint32_to_native(&tmp[0]);
        detail::prepare_key(impl_->rd_key, fh.key);
    }
    else
    {
        // initialize this otherwise operator== breaks
        fh.key = 0;
    }
    if(! detail::is_control(fh.op))
    {
        if(fh.op != detail::opcode::cont)
        {
            impl_->rd_size = 0;
            impl_->rd_op = fh.op;
        }
        else
        {
            if(impl_->rd_size > (std::numeric_limits<
                std::uint64_t>::max)() - fh.len)
            {
                // message size exceeds configured limit
                ec = error::message_too_big;
                return false;
            }
        }
        if(! impl_->rd_deflated())
        {
            if(impl_->rd_msg_max && beast::detail::sum_exceeds(
                impl_->rd_size, fh.len, impl_->rd_msg_max))
            {
                // message size exceeds configured limit
                ec = error::message_too_big;
                return false;
            }
        }
        impl_->rd_cont = ! fh.fin;
        impl_->rd_remain = fh.len;
    }
    b.consume(b.size() - buffer_size(cb));
    ec = {};
    return true;
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
void
stream<NextLayer, deflateSupported>::
write_close(DynamicBuffer& db, close_reason const& cr)
{
    using namespace boost::endian;
    detail::frame_header fh;
    fh.op = detail::opcode::close;
    fh.fin = true;
    fh.rsv1 = false;
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.len = cr.code == close_code::none ?
        0 : 2 + cr.reason.size();
    if(impl_->role == role_type::client)
    {
        fh.mask = true;
        fh.key = this->create_mask();
    }
    else
    {
        fh.mask = false;
    }
    detail::write(db, fh);
    if(cr.code != close_code::none)
    {
        detail::prepared_key key;
        if(fh.mask)
            detail::prepare_key(key, fh.key);
        {
            std::uint8_t tmp[2];
            ::new(&tmp[0]) big_uint16_buf_t{
                (std::uint16_t)cr.code};
            auto mb = db.prepare(2);
            net::buffer_copy(mb,
                net::buffer(tmp));
            if(fh.mask)
                detail::mask_inplace(mb, key);
            db.commit(2);
        }
        if(! cr.reason.empty())
        {
            auto mb = db.prepare(cr.reason.size());
            net::buffer_copy(mb,
                net::const_buffer(
                    cr.reason.data(), cr.reason.size()));
            if(fh.mask)
                detail::mask_inplace(mb, key);
            db.commit(cr.reason.size());
        }
    }
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
void
stream<NextLayer, deflateSupported>::
write_ping(DynamicBuffer& db,
    detail::opcode code, ping_data const& data)
{
    detail::frame_header fh;
    fh.op = code;
    fh.fin = true;
    fh.rsv1 = false;
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.len = data.size();
    fh.mask = impl_->role == role_type::client;
    if(fh.mask)
        fh.key = this->create_mask();
    detail::write(db, fh);
    if(data.empty())
        return;
    detail::prepared_key key;
    if(fh.mask)
        detail::prepare_key(key, fh.key);
    auto mb = db.prepare(data.size());
    net::buffer_copy(mb,
        net::const_buffer(
            data.data(), data.size()));
    if(fh.mask)
        detail::mask_inplace(mb, key);
    db.commit(data.size());
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class Decorator>
request_type
stream<NextLayer, deflateSupported>::
build_request(detail::sec_ws_key_type& key,
    string_view host, string_view target,
        Decorator const& decorator)
{
    request_type req;
    req.target(target);
    req.version(11);
    req.method(http::verb::get);
    req.set(http::field::host, host);
    req.set(http::field::upgrade, "websocket");
    req.set(http::field::connection, "upgrade");
    detail::make_sec_ws_key(key);
    req.set(http::field::sec_websocket_key, key);
    req.set(http::field::sec_websocket_version, "13");
    impl_->build_request_pmd(req);
    decorator(req);
    if(! req.count(http::field::user_agent))
        req.set(http::field::user_agent,
            BOOST_BEAST_VERSION_STRING);
    return req;
}

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator, class Decorator>
response_type
stream<NextLayer, deflateSupported>::
build_response(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    Decorator const& decorator,
    error_code& result)
{
    auto const decorate =
        [&decorator](response_type& res)
        {
            decorator(res);
            if(! res.count(http::field::server))
            {
                BOOST_STATIC_ASSERT(sizeof(BOOST_BEAST_VERSION_STRING) < 20);
                static_string<20> s(BOOST_BEAST_VERSION_STRING);
                res.set(http::field::server, s);
            }
        };
    auto err =
        [&](error e)
        {
            result = e;
            response_type res;
            res.version(req.version());
            res.result(http::status::bad_request);
            res.body() = result.message();
            res.prepare_payload();
            decorate(res);
            return res;
        };
    if(req.version() != 11)
        return err(error::bad_http_version);
    if(req.method() != http::verb::get)
        return err(error::bad_method);
    if(! req.count(http::field::host))
        return err(error::no_host);
    {
        auto const it = req.find(http::field::connection);
        if(it == req.end())
            return err(error::no_connection);
        if(! http::token_list{it->value()}.exists("upgrade"))
            return err(error::no_connection_upgrade);
    }
    {
        auto const it = req.find(http::field::upgrade);
        if(it == req.end())
            return err(error::no_upgrade);
        if(! http::token_list{it->value()}.exists("websocket"))
            return err(error::no_upgrade_websocket);
    }
    string_view key;
    {
        auto const it = req.find(http::field::sec_websocket_key);
        if(it == req.end())
            return err(error::no_sec_key);
        key = it->value();
        if(key.size() > detail::sec_ws_key_type::max_size_n)
            return err(error::bad_sec_key);
    }
    {
        auto const it = req.find(http::field::sec_websocket_version);
        if(it == req.end())
            return err(error::no_sec_version);
        if(it->value() != "13")
        {
            response_type res;
            res.result(http::status::upgrade_required);
            res.version(req.version());
            res.set(http::field::sec_websocket_version, "13");
            result = error::bad_sec_version;
            res.body() = result.message();
            res.prepare_payload();
            decorate(res);
            return res;
        }
    }

    response_type res;
    res.result(http::status::switching_protocols);
    res.version(req.version());
    res.set(http::field::upgrade, "websocket");
    res.set(http::field::connection, "upgrade");
    {
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        res.set(http::field::sec_websocket_accept, acc);
    }
    impl_->build_response_pmd(res, req);
    decorate(res);
    result = {};
    return res;
}

// Called when the WebSocket Upgrade response is received
template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
on_response(
    response_type const& res,
    detail::sec_ws_key_type const& key,
    error_code& ec)
{
    auto const err =
        [&](error e)
        {
            ec = e;
        };
    if(res.result() != http::status::switching_protocols)
        return err(error::upgrade_declined);
    if(res.version() != 11)
        return err(error::bad_http_version);
    {
        auto const it = res.find(http::field::connection);
        if(it == res.end())
            return err(error::no_connection);
        if(! http::token_list{it->value()}.exists("upgrade"))
            return err(error::no_connection_upgrade);
    }
    {
        auto const it = res.find(http::field::upgrade);
        if(it == res.end())
            return err(error::no_upgrade);
        if(! http::token_list{it->value()}.exists("websocket"))
            return err(error::no_upgrade_websocket);
    }
    {
        auto const it = res.find(http::field::sec_websocket_accept);
        if(it == res.end())
            return err(error::no_sec_accept);
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        if(acc.compare(it->value()) != 0)
            return err(error::bad_sec_accept);
    }

    ec = {};
    impl_->on_response_pmd(res);
    impl_->open(role_type::client);
}

// _Fail the WebSocket Connection_
template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
do_fail(
    std::uint16_t code,         // if set, send a close frame first
    error_code ev,              // error code to use upon success
    error_code& ec)             // set to the error, else set to ev
{
    BOOST_ASSERT(ev);
    impl_->status_ = status::closing;
    if(code != close_code::none && ! impl_->wr_close)
    {
        impl_->wr_close = true;
        detail::frame_buffer fb;
        write_close<
            flat_static_buffer_base>(fb, code);
        net::write(impl_->stream, fb.data(), ec);
        if(! impl_->check_ok(ec))
            return;
    }
    using beast::websocket::teardown;
    teardown(impl_->role, impl_->stream, ec);
    if(ec == net::error::eof)
    {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
    }
    if(! ec)
        ec = ev;
    if(ec && ec != error::closed)
        impl_->status_ = status::failed;
    else
        impl_->status_ = status::closed;
    impl_->close();
}

} // websocket
} // beast
} // boost

#endif
