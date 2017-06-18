//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_STREAM_IPP
#define BEAST_WEBSOCKET_IMPL_STREAM_IPP

#include <beast/websocket/rfc6455.hpp>
#include <beast/websocket/teardown.hpp>
#include <beast/websocket/detail/hybi13.hpp>
#include <beast/websocket/detail/pmd_extension.hpp>
#include <beast/version.hpp>
#include <beast/http/read.hpp>
#include <beast/http/write.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/core/buffer_cat.hpp>
#include <beast/core/buffer_prefix.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/static_buffer.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <iostream>

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
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid server_max_window_bits"});
    if( o.client_max_window_bits > 15 ||
        o.client_max_window_bits < 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid client_max_window_bits"});
    if( o.compLevel < 0 ||
        o.compLevel > 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid compLevel"});
    if( o.memLevel < 1 ||
        o.memLevel > 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid memLevel"});
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
    http::request_parser<http::empty_body> p;
    http::read_header(next_layer(),
        stream_.buffer(), p, ec);
    if(ec)
        return;
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
    if(res.result() != http::status::switching_protocols)
    {
        ec = error::handshake_failed;
        // VFALCO TODO Respect keep alive setting, perform
        //             teardown if Connection: close.
        return;
    }
    pmd_read(pmd_config_, req);
    open(detail::role_type::server);
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
do_handshake(response_type* res_p,
    string_view host,
        string_view target,
            RequestDecorator const& decorator,
                error_code& ec)
{
    response_type res;
    reset();
    detail::sec_ws_key_type key;
    {
        auto const req = build_request(
            key, host, target, decorator);
        pmd_read(pmd_config_, req);
        http::write(stream_, req, ec);
    }
    if(ec)
        return;
    http::read(next_layer(), stream_.buffer(), res, ec);
    if(ec)
        return;
    do_response(res, key, ec);
    if(res_p)
        *res_p = std::move(res);
}

template<class NextLayer>
template<class Decorator>
request_type
stream<NextLayer>::
build_request(detail::sec_ws_key_type& key,
    string_view host,
        string_view target,
            Decorator const& decorator)
{
    request_type req;
    req.target(target);
    req.version = 11;
    req.method(http::verb::get);
    req.set(http::field::host, host);
    req.set(http::field::upgrade, "websocket");
    req.set(http::field::connection, "upgrade");
    detail::make_sec_ws_key(key, maskgen_);
    req.set(http::field::sec_websocket_key, key);
    req.set(http::field::sec_websocket_version, "13");
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
        detail::pmd_write(req, config);
    }
    decorator(req);
    if(! req.count(http::field::user_agent))
        req.set(http::field::user_agent,
            BEAST_VERSION_STRING);
    return req;
}

template<class NextLayer>
template<class Fields, class Decorator>
response_type
stream<NextLayer>::
build_response(http::header<true, Fields> const& req,
    Decorator const& decorator)
{
    auto const decorate =
        [&decorator](response_type& res)
        {
            decorator(res);
            if(! res.count(http::field::server))
            {
                BOOST_STATIC_ASSERT(sizeof(BEAST_VERSION_STRING) < 20);
                static_string<20> s(BEAST_VERSION_STRING);
                res.set(http::field::server, s);
            }
        };
    auto err =
        [&](std::string const& text)
        {
            response_type res;
            res.version = req.version;
            res.result(http::status::bad_request);
            res.body = text;
            res.prepare();
            decorate(res);
            return res;
        };
    if(req.version < 11)
        return err("HTTP version 1.1 required");
    if(req.method() != http::verb::get)
        return err("Wrong method");
    if(! is_upgrade(req))
        return err("Expected Upgrade request");
    if(! req.count(http::field::host))
        return err("Missing Host");
    if(! req.count(http::field::sec_websocket_key))
        return err("Missing Sec-WebSocket-Key");
    if(! http::token_list{req[http::field::upgrade]}.exists("websocket"))
        return err("Missing websocket Upgrade token");
    auto const key = req[http::field::sec_websocket_key];
    if(key.size() > detail::sec_ws_key_type::max_size_n)
        return err("Invalid Sec-WebSocket-Key");
    {
        auto const version =
            req[http::field::sec_websocket_version];
        if(version.empty())
            return err("Missing Sec-WebSocket-Version");
        if(version != "13")
        {
            response_type res;
            res.result(http::status::upgrade_required);
            res.version = req.version;
            res.set(http::field::sec_websocket_version, "13");
            res.prepare();
            decorate(res);
            return res;
        }
    }

    response_type res;
    {
        detail::pmd_offer offer;
        detail::pmd_offer unused;
        pmd_read(offer, req);
        pmd_negotiate(res, unused, offer, pmd_opts_);
    }
    res.result(http::status::switching_protocols);
    res.version = req.version;
    res.set(http::field::upgrade, "websocket");
    res.set(http::field::connection, "upgrade");
    {
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        res.set(http::field::sec_websocket_accept, acc);
    }
    decorate(res);
    return res;
}

template<class NextLayer>
void
stream<NextLayer>::
do_response(http::header<false> const& res,
    detail::sec_ws_key_type const& key, error_code& ec)
{
    bool const success = [&]()
    {
        if(res.version < 11)
            return false;
        if(res.result() != http::status::switching_protocols)
            return false;
        if(! http::token_list{res[http::field::connection]}.exists("upgrade"))
            return false;
        if(! http::token_list{res[http::field::upgrade]}.exists("websocket"))
            return false;
        if(res.count(http::field::sec_websocket_accept) != 1)
            return false;
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        if(acc.compare(
                res[http::field::sec_websocket_accept]) != 0)
            return false;
        return true;
    }();
    if(! success)
    {
        ec = error::handshake_failed;
        return;
    }
    ec = {};
    detail::pmd_offer offer;
    pmd_read(offer, res);
    // VFALCO see if offer satisfies pmd_config_,
    //        return an error if not.
    pmd_config_ = offer; // overwrite for now
    open(detail::role_type::client);
}

} // websocket
} // beast

#endif
