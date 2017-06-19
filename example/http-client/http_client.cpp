//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_http_client

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
    // A helper for reporting errors
    auto const fail =
        [](std::string what, beast::error_code ec)
        {
            std::cerr << what << ": " << ec.message() << std::endl;
            std::cerr.flush();
            return EXIT_FAILURE;
        };

    beast::error_code ec;

    // Set up an asio socket
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r{ios};
    boost::asio::ip::tcp::socket sock{ios};

    // Look up the domain name
    std::string const host = "www.example.com";
    auto const lookup = r.resolve({host, "http"}, ec);
    if(ec)
        return fail("resolve", ec);

    // Make the connection on the IP address we get from a lookup
    boost::asio::connect(sock, lookup, ec);
    if(ec)
        return fail("connect", ec);

    // Set up an HTTP GET request message
    beast::http::request<beast::http::string_body> req;
    req.method(beast::http::verb::get);
    req.target("/");
    req.version = 11;
    req.set(beast::http::field::host, host + ":" +
        boost::lexical_cast<std::string>(sock.remote_endpoint().port()));
    req.set(beast::http::field::user_agent, "Beast");
    req.prepare();

    // Write the HTTP request to the remote host
    beast::http::write(sock, req, ec);
    if(ec)
        return fail("write", ec);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    beast::http::response<beast::http::dynamic_body> res;

    // Read the response
    beast::http::read(sock, b, res, ec);
    if(ec)
        return fail("read", ec);

    // Write the message to standard out
    std::cout << res << std::endl;

    // Gracefully close the socket
    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if(ec)
        return fail("shutdown", ec);

    // If we get here then the connection is closed gracefully
    return EXIT_SUCCESS;
}

//]
