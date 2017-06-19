//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "urls_large_data.hpp"

#include <beast/core/multi_buffer.hpp>
#include <beast/http.hpp>
#include <beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>

template<class String>
void
err(beast::error_code const& ec, String const& what)
{
    std::cerr << what << ": " << ec.message() << std::endl;
}

/*  This simple program just visits a list with a few
    thousand domain names and tries to retrieve and print
    the home page of each site.
*/
int
main(int, char const*[])
{
    // A helper for reporting errors
    auto const fail =
        [](std::string what, beast::error_code ec)
        {
            std::cerr << what << ": " << ec.message() << std::endl;
            std::cerr.flush();
            return EXIT_FAILURE;
        };

    // Obligatory Asio variable
    boost::asio::io_service ios;

    // Loop over all the URLs
    for(auto const& host : urls_large_data())
    {
        beast::error_code ec;

        // Look up the domain name
        boost::asio::ip::tcp::resolver r(ios);
        auto lookup = r.resolve(boost::asio::ip::tcp::resolver::query{host, "http"}, ec);
        if(ec)
        {
            fail("resolve", ec);
            continue;
        }

        // Now create a socket and connect
        boost::asio::ip::tcp::socket sock(ios);
        boost::asio::connect(sock, lookup, ec);
        if(ec)
        {
            fail("connect", ec);
            continue;
        }

        // Grab the remote endpoint
        auto ep = sock.remote_endpoint(ec);
        if(ec)
        {
            fail("remote_endpoint", ec);
            continue;
        }

        // Set up an HTTP GET request
        beast::http::request<beast::http::string_body> req;
        req.version = 11;
        req.method(beast::http::verb::get);
        req.target("/");
        req.set(beast::http::field::host, host + std::string(":") +
            boost::lexical_cast<std::string>(ep.port()));
        req.set(beast::http::field::user_agent, BEAST_VERSION_STRING);

        // Set the Connection: close field, this way the server will close
        // the connection. This consumes less resources (no TIME_WAIT) because
        // of the graceful close. It also makes things go a little faster.
        //
        req.set(beast::http::field::connection, "close");

        // Send the GET request
        beast::http::write(sock, req, ec);
        if(ec == beast::http::error::end_of_stream)
        {
            // This special error received on a write indicates that the
            // semantics of the sent message are such that the connection
            // should be closed after the response is done. We do a TCP/IP
            // "half-close" here to shut down our end.
            //
            sock.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            if(ec)
                return fail("shutdown", ec);
        }
        if(ec)
        {
            fail("write", ec);
            continue;
        }

        // This buffer is needed for reading
        beast::multi_buffer b;

        // The response will go into this object
        beast::http::response<beast::http::string_body> res;

        // Read the response
        beast::http::read(sock, b, res, ec);
        if(ec == beast::http::error::end_of_stream)
        {
            // This special error means that the other end closed the socket,
            // which is what we want since we asked for Connection: close.
            // However, we are going through a rather large number of servers
            // and sometimes they misbehave.
            ec = {};
        }
        else if(ec)
        {
            fail("read", ec);
            continue;
        }

        // Now we do the other half of the close,
        // which is to shut down the receiver. 
        sock.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
        if(ec)
            return fail("shutdown", ec);

        std::cout << res << std::endl;
    }
}
