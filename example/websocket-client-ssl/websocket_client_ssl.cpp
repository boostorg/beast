//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "../common/root_certificates.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

using tcp = boost::asio::ip::tcp; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
namespace websocket = boost::beast::websocket; // from <boost/beast/websocket.hpp>

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

    // Set up an asio socket to connect to a remote host
    boost::asio::io_service ios;
    tcp::resolver r{ios};
    tcp::socket sock{ios};

    // Look up the domain name
    std::string const host = "echo.websocket.org";
    auto const lookup = r.resolve({host, "https"}, ec);
    if(ec)
        return fail("resolve", ec);

    // Make the connection on the IP address we get from a lookup
    boost::asio::connect(sock, lookup, ec);
    if(ec)
        return fail("connect", ec);

    // Create the required ssl context
    ssl::context ctx{ssl::context::sslv23_client};
    
    // This holds the root certificate used for verification
    load_root_certificates(ctx, ec);
    if(ec)
        return fail("certificate", ec);

    // Wrap the now-connected socket in an SSL stream
    using stream_type = ssl::stream<tcp::socket&>;
    stream_type stream{sock, ctx};
    stream.set_verify_mode(ssl::verify_none);

    // Perform SSL handshaking
    stream.handshake(ssl::stream_base::client, ec);
    if(ec)
        return fail("ssl handshake", ec);

    // Now wrap the handshaked SSL stream in a websocket stream
    websocket::stream<stream_type&> ws{stream};

    // Perform the websocket handshake
    ws.handshake(host, "/", ec);
    if(ec)
        return fail("handshake", ec);

    // Send a message
    ws.write(boost::asio::buffer("Hello, world!"), ec);
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

    return EXIT_SUCCESS;
}
