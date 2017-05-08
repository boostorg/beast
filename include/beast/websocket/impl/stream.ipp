//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_STREAM_IPP
#define BEAST_WEBSOCKET_IMPL_STREAM_IPP

#include <beast/websocket/teardown.hpp>
#include <beast/websocket/detail/hybi13.hpp>
#include <beast/websocket/detail/pmd_extension.hpp>
#include <beast/version.hpp>
#include <beast/http/read.hpp>
#include <beast/http/write.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/core/buffer_cat.hpp>
#include <beast/core/buffer_concepts.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/prepare_buffers.hpp>
#include <beast/core/static_buffer.hpp>
#include <beast/core/stream_concepts.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
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
set_option(permessage_deflate const& o)
{
    if( o.server_max_window_bits > 15 ||
        o.server_max_window_bits < 9)
        throw std::invalid_argument{
            "invalid server_max_window_bits"};
    if( o.client_max_window_bits > 15 ||
        o.client_max_window_bits < 9)
        throw std::invalid_argument{
            "invalid client_max_window_bits"};
    if( o.compLevel < 0 ||
        o.compLevel > 9)
        throw std::invalid_argument{
            "invalid compLevel"};
    if( o.memLevel < 1 ||
        o.memLevel > 9)
        throw std::invalid_argument{
            "invalid memLevel"};
    pmd_opts_ = o;
}

//------------------------------------------------------------------------------

template<class NextLayer>
void
stream<NextLayer>::
reset()
{
    failed_ = false;
    rd_.cont = false;
    wr_close_ = false;
    wr_.cont = false;
    wr_block_ = nullptr;    // should be nullptr on close anyway
    ping_data_ = nullptr;   // should be nullptr on close anyway

    stream_.buffer().consume(
        stream_.buffer().size());
}

template<class NextLayer>
template<class Decorator>
void
stream<NextLayer>::
do_accept(
    Decorator const& decorator, error_code& ec)
{
    http::header_parser<true, http::fields> p;
    auto const bytes_used = http::read_some(
        next_layer(), stream_.buffer(), p, ec);
    if(ec)
        return;
    BOOST_ASSERT(p.got_header());
    stream_.buffer().consume(bytes_used);
    do_accept(p.get(), decorator, ec);
}

template<class NextLayer>
template<class Fields, class Decorator>
void
stream<NextLayer>::
do_accept(http::header<true, Fields> const& req,
    Decorator const& decorator, error_code& ec)
{
    auto const res = build_response(req, decorator);
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
    pmd_read(pmd_config_, req.fields);
    open(detail::role_type::server);
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
do_handshake(response_type* res_p,
    string_view const& host,
        string_view const& target,
            RequestDecorator const& decorator,
                error_code& ec)
{
    response_type res;
    reset();
    detail::sec_ws_key_type key;
    {
        auto const req = build_request(
            key, host, target, decorator);
        pmd_read(pmd_config_, req.fields);
        http::write(stream_, req, ec);
    }
    if(ec)
        return;
    http::read(next_layer(), stream_.buffer(), res, ec);
    if(ec)
        return;
    do_response(res, key, ec);
    if(res_p)
        swap(res, *res_p);
}

template<class NextLayer>
template<class Decorator>
request_type
stream<NextLayer>::
build_request(detail::sec_ws_key_type& key,
    string_view const& host,
        string_view const& target,
            Decorator const& decorator)
{
    request_type req;
    req.target(target);
    req.version = 11;
    req.method("GET");
    req.fields.insert("Host", host);
    req.fields.insert("Upgrade", "websocket");
    req.fields.insert("Connection", "upgrade");
    detail::make_sec_ws_key(key, maskgen_);
    req.fields.insert("Sec-WebSocket-Key", key);
    req.fields.insert("Sec-WebSocket-Version", "13");
    if(pmd_opts_.client_enable)
    {
        detail::pmd_offer config;
        config.accept = true;
        config.server_max_window_bits =
            pmd_opts_.server_max_window_bits;
        config.client_max_window_bits =
            pmd_opts_.client_max_window_bits;
        config.server_no_context_takeover =
            pmd_opts_.server_no_context_takeover;
        config.client_no_context_takeover =
            pmd_opts_.client_no_context_takeover;
        detail::pmd_write(
            req.fields, config);
    }
    decorator(req);
    if(! req.fields.exists("User-Agent"))
    {
        static_string<20> s(BEAST_VERSION_STRING);
        req.fields.insert("User-Agent", s);
    }
    return req;
}

template<class NextLayer>
template<class Decorator>
response_type
stream<NextLayer>::
build_response(request_type const& req,
    Decorator const& decorator)
{
    auto const decorate =
        [&decorator](response_type& res)
        {
            decorator(res);
            if(! res.fields.exists("Server"))
            {
                static_string<20> s(BEAST_VERSION_STRING);
                res.fields.insert("Server", s);
            }
        };
    auto err =
        [&](std::string const& text)
        {
            response_type res;
            res.status = 400;
            res.reason(http::reason_string(res.status));
            res.version = req.version;
            res.body = text;
            prepare(res);
            decorate(res);
            return res;
        };
    if(req.version < 11)
        return err("HTTP version 1.1 required");
    if(req.method() != "GET")
        return err("Wrong method");
    if(! is_upgrade(req))
        return err("Expected Upgrade request");
    if(! req.fields.exists("Host"))
        return err("Missing Host");
    if(! req.fields.exists("Sec-WebSocket-Key"))
        return err("Missing Sec-WebSocket-Key");
    if(! http::token_list{req.fields["Upgrade"]}.exists("websocket"))
        return err("Missing websocket Upgrade token");
    auto const key = req.fields["Sec-WebSocket-Key"];
    if(key.size() > detail::sec_ws_key_type::max_size_n)
        return err("Invalid Sec-WebSocket-Key");
    {
        auto const version =
            req.fields["Sec-WebSocket-Version"];
        if(version.empty())
            return err("Missing Sec-WebSocket-Version");
        if(version != "13")
        {
            response_type res;
            res.status = 426;
            res.reason(http::reason_string(res.status));
            res.version = req.version;
            res.fields.insert("Sec-WebSocket-Version", "13");
            prepare(res);
            decorate(res);
            return res;
        }
    }

    response_type res;
    {
        detail::pmd_offer offer;
        detail::pmd_offer unused;
        pmd_read(offer, req.fields);
        pmd_negotiate(
            res.fields, unused, offer, pmd_opts_);
    }
    res.status = 101;
    res.reason(http::reason_string(res.status));
    res.version = req.version;
    res.fields.insert("Upgrade", "websocket");
    res.fields.insert("Connection", "upgrade");
    {
        detail::sec_ws_accept_type accept;
        detail::make_sec_ws_accept(accept, key);
        res.fields.insert("Sec-WebSocket-Accept", accept);
    }
    decorate(res);
    return res;
}

template<class NextLayer>
void
stream<NextLayer>::
do_response(http::response_header const& res,
    detail::sec_ws_key_type const& key, error_code& ec)
{
    bool const success = [&]()
    {
        if(res.version < 11)
            return false;
        if(res.status != 101)
            return false;
        if(! is_upgrade(res))
            return false;
        if(! http::token_list{res.fields["Upgrade"]}.exists("websocket"))
            return false;
        if(! res.fields.exists("Sec-WebSocket-Accept"))
            return false;
        detail::sec_ws_accept_type accept;
        detail::make_sec_ws_accept(accept, key);
        if(accept.compare(
                res.fields["Sec-WebSocket-Accept"]) != 0)
            return false;
        return true;
    }();
    if(! success)
    {
        ec = error::handshake_failed;
        return;
    }
    detail::pmd_offer offer;
    pmd_read(offer, res.fields);
    // VFALCO see if offer satisfies pmd_config_,
    //        return an error if not.
    pmd_config_ = offer; // overwrite for now
    open(detail::role_type::client);
}

} // websocket
} // beast

#endif
