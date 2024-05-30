//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//[snippet_core_1a

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <thread>

//]

using namespace boost::beast;

namespace doc_core_snippets {

void fxx()
{
//[snippet_core_1b
//
using namespace boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

net::io_context ioc;
net::any_io_executor work =
    net::require(ioc.get_executor(),
        net::execution::outstanding_work.tracked);
std::thread t{[&](){ ioc.run(); }};

error_code ec;
tcp::socket sock{ioc};

//]
    boost::ignore_unused(ec);

    {
//[snippet_core_2

// The resolver is used to look up IP addresses and port numbers from a domain and service name pair
tcp::resolver r{ioc};

// A socket represents the local end of a connection between two peers
tcp::socket stream{ioc};

// Establish a connection before sending and receiving data
net::connect(stream, r.resolve("www.example.com", "http"));

// At this point `stream` is a connected to a remote
// host and may be used to perform stream operations.

//]
    }

} // fxx()

//------------------------------------------------------------------------------

//[snippet_core_3

template<class SyncWriteStream>
void write_string(SyncWriteStream& stream, string_view s)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream type requirements not met");
    net::write(stream, net::const_buffer(s.data(), s.size()));
}

//]

void ssl_tls_shutdown()
{
    net::io_context ioc;
    net::ssl::context ctx(net::ssl::context::tlsv12);
    net::ssl::stream<tcp_stream> stream(ioc, ctx);
    flat_buffer buffer;
    http::response<http::dynamic_body> res;
    auto log = [](error_code){};

    {
//[snippet_core_4
// Receive the HTTP response
http::read(stream, buffer, res);

// Gracefully shutdown the SSL/TLS connection
error_code ec;
stream.shutdown(ec);
// Non-compliant servers don't participate in the SSL/TLS shutdown process and
// close the underlying transport layer. This causes the shutdown operation to
// complete with a `stream_truncated` error. One might decide not to log such
// errors as there are many non-compliant servers in the wild.
if(ec != net::ssl::error::stream_truncated)
    log(ec);
//]
    }

    {
//[snippet_core_5
// Use an HTTP response parser to have more control
http::response_parser<http::dynamic_body> parser;

error_code ec;
// Receive the HTTP response until the end or until an error occurs
http::read(stream, buffer, parser, ec);

// Try to manually commit the EOF, the resulting message body would be
// vulnerable to truncation attacks.
if(parser.need_eof() && ec == net::ssl::error::stream_truncated)
    parser.put_eof(ec); // Override the error_code

if(ec)
    throw system_error{ec};

// Access the HTTP response inside the parser
std::cout << parser.get() << std::endl;


// Gracefully shutdown the SSL/TLS connection
stream.shutdown(ec); // Override the error_code
// Non-compliant servers don't participate in the SSL/TLS shutdown process and
// close the underlying transport layer. This causes the shutdown operation to
// complete with a `stream_truncated` error. One might decide not to log such
// errors as there are many non-compliant servers in the wild.
if(ec != net::ssl::error::stream_truncated)
    log(ec);
//]
    }
}

} // doc_core_snippets
