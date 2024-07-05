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
// Example: WebSocket server, coroutine
//
//------------------------------------------------------------------------------

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;

// Echoes back all received WebSocket messages
net::awaitable<void>
do_session(websocket::stream<beast::tcp_stream> stream)
{
    // Set suggested timeout settings for the websocket
    stream.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    stream.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(
                http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-coro");
        }));

    // Accept the websocket handshake
    co_await stream.async_accept();

    for(;;)
    {
        // This buffer will hold the incoming message
        beast::flat_buffer buffer;

        // Read a message
        auto [ec, _] = co_await stream.async_read(buffer, net::as_tuple);

        if(ec == websocket::error::closed)
            co_return;

        if(ec)
            throw boost::system::system_error{ ec };

        // Echo the message back
        stream.text(stream.got_text());
        co_await stream.async_write(buffer.data());
    }
}

// Accepts incoming connections and launches the sessions
net::awaitable<void>
do_listen(net::ip::tcp::endpoint endpoint)
{
    auto executor = co_await net::this_coro::executor;
    auto acceptor = net::ip::tcp::acceptor{ executor, endpoint };

    for(;;)
    {
        net::co_spawn(
            executor,
            do_session(websocket::stream<beast::tcp_stream>{
                co_await acceptor.async_accept() }),
            [](std::exception_ptr e)
            {
                if(e)
                {
                    try
                    {
                        std::rethrow_exception(e);
                    }
                    catch(std::exception& e)
                    {
                        std::cerr << "Error in session: " << e.what() << "\n";
                    }
                }
            });
    }
}

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if(argc != 4)
    {
        std::cerr
            << "Usage: websocket-server-awaitable <address> <port> <threads>\n"
            << "Example:\n"
            << "    websocket-server-awaitable 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port    = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    // The io_context is required for all I/O
    net::io_context ioc(threads);

    // Spawn a listening port
    net::co_spawn(
        ioc,
        do_listen(net::ip::tcp::endpoint{ address, port }),
        [](std::exception_ptr e)
        {
            if(e)
            {
                try
                {
                    std::rethrow_exception(e);
                }
                catch(std::exception const& e)
                {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
            }
        });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

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
