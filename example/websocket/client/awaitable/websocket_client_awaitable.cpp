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
// Example: WebSocket client, coroutine
//
//------------------------------------------------------------------------------

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;

// Sends a WebSocket message and prints the response
net::awaitable<void>
do_session(std::string host, std::string port, std::string text)
{
    auto executor = co_await net::this_coro::executor;
    auto resolver = net::ip::tcp::resolver{ executor };
    auto stream   = websocket::stream<beast::tcp_stream>{ executor };

    // Look up the domain name
    auto const results = co_await resolver.async_resolve(host, port);

    // Set a timeout on the operation
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    auto ep = co_await beast::get_lowest_layer(stream).async_connect(results);

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    host += ':' + std::to_string(ep.port());

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(stream).expires_never();

    // Set suggested timeout settings for the websocket
    stream.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    stream.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req)
        {
            req.set(
                http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-coro");
        }));

    // Perform the websocket handshake
    co_await stream.async_handshake(host, "/");

    // Send the message
    co_await stream.async_write(net::buffer(text));

    // This buffer will hold the incoming message
    beast::flat_buffer buffer;

    // Read a message into our buffer
    co_await stream.async_read(buffer);

    // Close the WebSocket connection
    co_await stream.async_close(websocket::close_code::normal);

    // If we get here then the connection is closed gracefully

    // The make_printable() function helps print a ConstBufferSequence
    std::cout << beast::make_printable(buffer.data()) << std::endl;
}

//------------------------------------------------------------------------------

int
main(int argc, char** argv)
{
    try
    {
        // Check command line arguments.
        if(argc != 4)
        {
            std::cerr
                << "Usage: websocket-client-awaitable <host> <port> <text>\n"
                << "Example:\n"
                << "    websocket-client-awaitable echo.websocket.org 80 \"Hello, world!\"\n";
            return EXIT_FAILURE;
        }
        auto const host = argv[1];
        auto const port = argv[2];
        auto const text = argv[3];

        // The io_context is required for all I/O
        net::io_context ioc;

        // Launch the asynchronous operation
        net::co_spawn(
            ioc,
            do_session(host, port, text),
            [](std::exception_ptr e)
            {
                if(e)
                    std::rethrow_exception(e);
            });

        // Run the I/O service. The call will return when
        // the socket is closed.
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
