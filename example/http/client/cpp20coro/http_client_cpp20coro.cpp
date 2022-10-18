//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
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
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <boost/scope_exit.hpp>
#include <coroutine>
#include <functional>
#include <ranges>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Performs an HTTP GET and prints the response
boost::asio::awaitable<void> do_get(
        const tcp::resolver::results_type& results,
        std::string const& host,
        std::string target,
        int version,
        net::io_context& ioc) 
{
    beast::error_code ec;

    beast::tcp_stream stream(ioc);

    BOOST_SCOPE_EXIT(&stream, &target) {
        if (stream.socket().is_open()) {
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            // not_connected happens sometimes
            // so don't bother reporting it.
            if(ec && ec != beast::errc::not_connected)
                throw boost::system::system_error(ec, "shutdown " + target);
        }
    } BOOST_SCOPE_EXIT_END

    // Make the connection on the IP address we get from a lookup
    co_await stream.async_connect(results, net::use_awaitable);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    co_await http::async_write(stream, req, net::use_awaitable);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    co_await http::async_read(stream, b, res, net::use_awaitable);

    // Write the message to standard out
    std::cout << res << std::endl;
}

net::awaitable<void> do_multiple_get(
        int count,
        const std::string& host,
        const std::string port,
        std::string target,
        int version,
        net::io_context& ioc) 
{

    // resolve hostname once
    tcp::resolver resolver(ioc);
    const auto results = co_await resolver.async_resolve(host, port, net::use_awaitable);


    // call multiple get in parallel
    for (const auto i : std::ranges::iota_view{0, count}) {
        const std::string target_i = target+std::to_string(i);
        net::co_spawn(ioc, do_get(results, host, target_i, version, ioc), 
                [target_i](std::exception_ptr eptr) {
                try {
                    if (eptr)
                        std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    std::cerr << "do_get(" << target_i <<") failed : " <<  e.what() << std::endl;
                }
                });
    }
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4 && argc != 5)
    {
        std::cerr <<
            "Usage: http-client-cpp20coro <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-cpp20coro www.example.com 80 /\n" <<
            "    http-client-cpp20coro www.example.com 80 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    net::co_spawn(ioc, do_multiple_get(100, host, port, target, version, ioc),
            [](std::exception_ptr eptr) {
                try {
                    if (eptr)
                        std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    std::cerr << "do_multiple_get() failed : " <<  e.what() << std::endl;
                }
            });

    // Run the I/O service. The call will return when
    // all get operations are complete.
    ioc.run();

    return EXIT_SUCCESS;
}
