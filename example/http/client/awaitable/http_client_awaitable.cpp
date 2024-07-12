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

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;

// Performs an HTTP GET and prints the response
net::awaitable<void>
do_session(std::string host, std::string port, std::string target, int version)
{
    auto executor = co_await net::this_coro::executor;
    auto resolver = net::ip::tcp::resolver{ executor };
    auto stream   = beast::tcp_stream{ executor };

    // Look up the domain name
    auto const results = co_await resolver.async_resolve(host, port);

    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    co_await stream.async_connect(results);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{ http::verb::get, target, version };
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    co_await http::async_write(stream, req);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer buffer;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    co_await http::async_read(stream, buffer, res);

    // Write the message to standard out
    std::cout << res << std::endl;

    // Gracefully close the socket
    beast::error_code ec;
    stream.socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if(ec && ec != beast::errc::not_connected)
        throw boost::system::system_error(ec, "shutdown");

    // If we get here then the connection is closed gracefully
}

//------------------------------------------------------------------------------

int
main(int argc, char** argv)
{
    try
    {
        // Check command line arguments.
        if(argc != 4 && argc != 5)
        {
            std::cerr << "Usage: http-client-awaitable <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n"
                      << "Example:\n"
                      << "    http-client-awaitable www.example.com 80 /\n"
                      << "    http-client-awaitable www.example.com 80 / 1.0\n";
            return EXIT_FAILURE;
        }
        auto const host   = argv[1];
        auto const port   = argv[2];
        auto const target = argv[3];
        auto const version =
            argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

        // The io_context is required for all I/O
        net::io_context ioc;

        // Launch the asynchronous operation
        net::co_spawn(
            ioc,
            do_session(host, port, target, version),
            // If the awaitable exists with an exception, it gets delivered here
            // as `e`. This can happen for regular errors, such as connection
            // drops.
            [](std::exception_ptr e)
            {
                if(e)
                    std::rethrow_exception(e);
            });

        // Run the I/O service. The call will return when
        // the get operation is complete.
        ioc.run();
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#else

int
main(int, char*[])
{
    std::printf("awaitables require C++20\n");
    return EXIT_FAILURE;
}

#endif
