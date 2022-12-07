//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>

using namespace boost;
namespace websocket = boost::beast::websocket;

// just compile this.
void test_any_comletion_handler_for_http(
    websocket::stream<asio::ip::tcp::socket> & stream,
    beast::flat_static_buffer_base & buf,
    beast::http::request<beast::http::empty_body> & req,
    websocket::response_type & res,
    asio::any_completion_handler<void(system::error_code, std::size_t)> handler,
    asio::any_completion_handler<void(system::error_code)> handler2)
{
  stream.async_accept(std::move(handler2));
  stream.async_accept(asio::const_buffer(), std::move(handler2));
  stream.async_accept(req, std::move(handler2));
  stream.async_close(websocket::close_code::bad_payload, std::move(handler2));

  stream.async_handshake("", "/", std::move(handler2));
  stream.async_handshake(res, "", "/", std::move(handler2));

  stream.async_ping(websocket::ping_data{}, std::move(handler2));
  stream.async_pong(websocket::ping_data{}, std::move(handler2));

  stream.async_read(buf, std::move(handler));
  stream.async_read_some(buf.data(), std::move(handler));

  stream.async_write(buf.cdata(), std::move(handler));
  stream.async_write_some(true, buf.cdata(), std::move(handler));

}

