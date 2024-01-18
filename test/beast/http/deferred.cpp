//
// Copyright (c) 2023 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/deferred.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>

using namespace boost;
namespace http = boost::beast::http;

#if !defined(BOOST_NO_CXX14)

// just compile this.
void test_deferred_for_http(
    asio::ip::tcp::socket & stream,
    beast::flat_static_buffer_base & buf,
    http::basic_parser<true> & parser,
    http::serializer<true, http::empty_body> & ser,
    http::message<false, http::empty_body> & msg)
{
  (void)http::async_read(stream, buf, parser, asio::deferred);
  (void)http::async_read(stream, buf, msg, asio::deferred);
  (void)http::async_read_some(stream, buf, parser, asio::deferred);
  (void)http::async_read_header(stream, buf, parser, asio::deferred);

  (void)http::async_write(stream, ser, asio::deferred);
  (void)http::async_write(stream, msg, asio::deferred);
  (void)http::async_write_header(stream, ser, asio::deferred);
  (void)http::async_write_some(stream, ser, asio::deferred);
}

#endif
