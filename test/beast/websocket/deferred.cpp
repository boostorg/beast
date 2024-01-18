//
// Copyright (c) 2023 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/deferred.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>

using namespace boost;
namespace websocket = boost::beast::websocket;

#if !defined(BOOST_NO_CXX14)

// just compile this.
void test_deferred_for_websocket(
    websocket::stream<asio::ip::tcp::socket> & stream,
    beast::flat_static_buffer_base & buf,
    beast::http::request<beast::http::empty_body> & req,
    websocket::response_type & res)
{
  (void)stream.async_accept(asio::deferred);
  (void)stream.async_accept(asio::const_buffer(), asio::deferred);
  (void)stream.async_accept(req, asio::deferred);
  (void)stream.async_close(websocket::close_code::bad_payload, asio::deferred);

  (void)stream.async_handshake("", "/", asio::deferred);
  (void)stream.async_handshake(res, "", "/", asio::deferred);

  (void)stream.async_ping(websocket::ping_data{}, asio::deferred);
  (void)stream.async_pong(websocket::ping_data{}, asio::deferred);

  (void)stream.async_read(buf, asio::deferred);
  (void)stream.async_read_some(buf.data(), asio::deferred);

  (void)stream.async_write(buf.cdata(), asio::deferred);
  (void)stream.async_write_some(true, buf.cdata(), asio::deferred);
}

#endif
