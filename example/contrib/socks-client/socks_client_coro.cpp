//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: SOCKS proxy client, asynchronous stackful coroutines
//
//------------------------------------------------------------------------------

#include <socks/handshake.hpp>
#include <socks/uri.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
void
do_session(
    std::string const& host,
    std::string const& port,
    std::string const& target,
    int version,
    std::string const& socks_server,
    net::io_context& ioc,
    boost::asio::yield_context yield)
{
    beast::error_code ec;

    // These objects perform our I/O
    tcp::resolver resolver{ioc};
    tcp::socket socket{ioc};

    socks::uri socks_url;
    if (!socks_url.parse(socks_server))
    {
        std::cerr << "parse socks url error\n";
        return;
    }

    // socks handshake.
    int socks_version = socks_url.scheme() == "socks4" ? 4 : 0;
    socks_version = socks_url.scheme() == "socks5" ? 5 : socks_version;
    if (socks_version == 0)
    {
        std::cerr << "incorrect socks version\n";
        return;
    }

    // Look up the domain name
    auto const results = resolver.async_resolve(
        std::string(socks_url.host()),
        std::string(socks_url.port()),
        yield[ec]);
    if (ec)
        return fail(ec, "resolve");

    // Make the connection on the IP address we get from a lookup
    net::async_connect(socket, results, yield[ec]);
    if (ec)
        return fail(ec, "connect");

    if(socks_version == 4)
        socks::async_handshake_v4(
            socket,
            host,
            static_cast<unsigned short>(std::atoi(port.c_str())),
            std::string(socks_url.username()),
            yield[ec]);
    else
        socks::async_handshake_v5(
            socket,
            host,
            static_cast<unsigned short>(std::atoi(port.c_str())),
            std::string(socks_url.username()),
            std::string(socks_url.password()),
            true,
            yield[ec]);

    if (ec)
        return fail(ec, "socks async_handshake");

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Send the HTTP request to the remote host
    http::async_write(socket, req, yield[ec]);
    if(ec)
        return fail(ec, "write");

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    http::async_read(socket, b, res, yield[ec]);
    if(ec)
        return fail(ec, "read");

    // Write the message to standard out
    std::cout << res << std::endl;

    // Gracefully close the socket
    socket.shutdown(tcp::socket::shutdown_both, ec);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if(ec && ec != beast::errc::not_connected)
        return fail(ec, "shutdown");

    // If we get here then the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 5)
    {
        std::cerr <<
            "Usage: socks-client-coro <host> <port> <target> <socks[4|5]://[[user]:password@]server:port>\n" <<
            "Example:\n" <<
            "    socks-client-coro www.example.com 80 / socks5://socks5server.com:1080\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = 11;
    auto const socks_server = argv[4];

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    boost::asio::spawn(ioc, std::bind(
        &do_session,
        std::string(host),
        std::string(port),
        std::string(target),
        version,
        socks_server,
        std::ref(ioc),
        std::placeholders::_1));

    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();

    return EXIT_SUCCESS;
}
