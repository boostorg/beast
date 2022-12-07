//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>

using namespace boost;
namespace http = boost::beast::http;

// just compile this.
void test_any_comletion_handler_for_http(
    asio::ip::tcp::socket & stream,
    beast::flat_static_buffer_base & buf,
    http::basic_parser<true> & parser,
    http::serializer<true, http::empty_body> & ser,
    http::message<false, http::empty_body> & msg,
    asio::any_completion_handler<void(system::error_code, std::size_t)> handler)
{
  http::async_read(stream, buf, parser, std::move(handler));
  http::async_read(stream, buf, msg, std::move(handler));
  http::async_read_some(stream, buf, parser, std::move(handler));
  http::async_read_header(stream, buf, parser, std::move(handler));

  http::async_write(stream, ser, std::move(handler));
  http::async_write(stream, msg, std::move(handler));
  http::async_write_header(stream, ser, std::move(handler));
  http::async_write_some(stream, ser, std::move(handler));
}

