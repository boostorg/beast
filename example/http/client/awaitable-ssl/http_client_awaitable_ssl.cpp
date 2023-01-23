//
// Copyright (c) 2022 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, coroutine
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include "example/common/root_certificates.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Performs an HTTP GET and prints the response
net::awaitable<void>
do_session(
    std::string host,
    std::string port,
    std::string target,
    int version,
    ssl::context& ctx)
{
    // These objects perform our I/O
    // They use an executor with a default completion token of use_awaitable
    // This makes our code easy, but will use exceptions as the default error handling,
    // i.e. if the connection drops, we might see an exception.
    // See async_shutdown for error handling with an error_code.
    auto resolver = net::use_awaitable.as_default_on(tcp::resolver(co_await net::this_coro::executor));
    using executor_with_default = net::use_awaitable_t<>::executor_with_default<net::any_io_executor>;
    using tcp_stream = typename beast::tcp_stream::rebind_executor<executor_with_default>::other;

    // We construct the ssl stream from the already rebound tcp_stream.
    beast::ssl_stream<tcp_stream> stream{
        net::use_awaitable.as_default_on(beast::tcp_stream(co_await net::this_coro::executor)),
        ctx};

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if(! SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        throw boost::system::system_error(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());

    // Look up the domain name
    auto const results = co_await resolver.async_resolve(host, port);

    // Set the timeout.
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    co_await beast::get_lowest_layer(stream).async_connect(results);

    // Set the timeout.
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    // Perform the SSL handshake
    co_await stream.async_handshake(ssl::stream_base::client);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Set the timeout.
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    co_await http::async_write(stream, req);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    co_await http::async_read(stream, b, res);

    // Write the message to standard out
    std::cout << res << std::endl;

    // Set the timeout.
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    // Gracefully close the stream - do not threat every error as an exception!
    auto [ec] = co_await stream.async_shutdown(net::as_tuple(net::use_awaitable));
    if (ec == net::error::eof)
    {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
    }
    if (ec)
        throw boost::system::system_error(ec, "shutdown");

    // If we get here then the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4 && argc != 5)
    {
        std::cerr <<
            "Usage: http-client-awaitable <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-awaitable www.example.com 443 /\n" <<
            "    http-client-awaitable www.example.com 443 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    // This holds the root certificate used for verification
    load_root_certificates(ctx);

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);


    // Launch the asynchronous operation
    net::co_spawn(
          ioc,
          do_session(host, port, target, version, ctx),
          // If the awaitable exists with an exception, it gets delivered here as `e`.
          // This can happen for regular errors, such as connection drops.
          [](std::exception_ptr e)
          {
            if (!e)
                return ;
            try
            {
                std::rethrow_exception(e);
            }
            catch(std::exception & ex)
            {
                std::cerr << "Error: " << ex.what() << "\n";
            }
          });

    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();

    return EXIT_SUCCESS;
}

#else

int main(int, char * [])
{
    std::printf("awaitables require C++20\n");
    return 1;
}

#endif
