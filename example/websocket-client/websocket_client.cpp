//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//[example_websocket_client

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

using tcp = boost::asio::ip::tcp;               // from <boost/asio.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

int main()
{
    // A helper for reporting errors
    auto const fail =
        [](std::string what, boost::beast::error_code ec)
        {
            std::cerr << what << ": " << ec.message() << std::endl;
            std::cerr.flush();
            return EXIT_FAILURE;
        };

    boost::system::error_code ec;

    // Set up an asio socket
    boost::asio::io_service ios;
    tcp::resolver r{ios};
    tcp::socket sock{ios};

    // Look up the domain name
    std::string const host = "echo.websocket.org";
    auto const lookup = r.resolve({host, "http"}, ec);
    if(ec)
        return fail("resolve", ec);

    // Make the connection on the IP address we get from a lookup
    boost::asio::connect(sock, lookup, ec);
    if(ec)
        return fail("connect", ec);

    // Wrap the now-connected socket in a websocket stream
    websocket::stream<tcp::socket&> ws{sock};

    // Perform the websocket handshake
    ws.handshake(host, "/", ec);
    if(ec)
        return fail("handshake", ec);

    // Send a message
    ws.write(boost::asio::buffer(std::string("Hello, world!")), ec);
    if(ec)
        return fail("write", ec);

    // This buffer will hold the incoming message
    boost::beast::multi_buffer b;

    // Read the message into our buffer
    ws.read(b, ec);
    if(ec)
        return fail("read", ec);

    // Close the WebSocket connection
    ws.close(websocket::close_code::normal, ec);
    if(ec)
        return fail("close", ec);

    // The buffers() function helps print a ConstBufferSequence
    std::cout << boost::beast::buffers(b.data()) << std::endl;

    // If we get here the connection was cleanly closed
    return EXIT_SUCCESS;
}

//]
