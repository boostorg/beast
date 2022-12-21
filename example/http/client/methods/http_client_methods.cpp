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
// Example: HTTP client, synchronous for every method on httpbin
//
//------------------------------------------------------------------------------

//[example_http_client_methods

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>


// perform a get request
void do_get(beast::tcp_stream & stream,
            http::request<http::string_body> & req,
            beast::flat_buffer buffer,
            http::response<http::dynamic_body> & res)
{
    req.target("/get");
    req.method(beast::http::verb::get);
    http::write(stream, req);
    http::read(stream, buffer, res);
}

// perform a head request
void do_head(beast::tcp_stream & stream,
            http::request<http::string_body> & req,
            beast::flat_buffer buffer,
            http::response<http::dynamic_body> & res)
{
    // we reuse the get endpoint
    req.target("/get");
    req.method(beast::http::verb::head);
    http::write(stream, req);

    /* the head response will send back a content-length
     * without a body. The other requests don't set content-length when not
     * sending a body back.
     *
     * the response parser doesn't know that we sent head,
     * so we need to manually make sure we're only reading the header
     * otherwise we're waiting forever for data.
     */

    http::response_parser<http::dynamic_body> p;
    http::read_header(stream, buffer, p);
    // move the result over
    res = p.release();
}


// perform a patch request
void do_patch(beast::tcp_stream & stream,
              http::request<http::string_body> & req,
              beast::flat_buffer buffer,
              http::response<http::dynamic_body> & res)
{
    req.target("/patch");
    req.method(beast::http::verb::patch);
    req.body() = "Some random patch data";
    req.prepare_payload(); // set content-length based on the body
    http::write(stream, req);
    http::read(stream, buffer, res);
}


// perform a put request
void do_put(beast::tcp_stream & stream,
            http::request<http::string_body> & req,
            beast::flat_buffer buffer,
            http::response<http::dynamic_body> & res)
{
    req.target("/put");
    req.method(beast::http::verb::put);
    req.body() = "Some random put data";
    req.prepare_payload(); // set content-length based on the body
    http::write(stream, req);
    http::read(stream, buffer, res);
}

// perform a post request
void do_post(beast::tcp_stream & stream,
             http::request<http::string_body> & req,
             beast::flat_buffer buffer,
             http::response<http::dynamic_body> & res)
{
    req.target("/post");
    req.method(beast::http::verb::post);
    req.body() = "Some random post data";
    req.prepare_payload(); // set content-length based on the body
    http::write(stream, req);
    http::read(stream, buffer, res);
}


// perform a delete request
void do_delete(beast::tcp_stream & stream,
               http::request<http::string_body> & req,
               beast::flat_buffer buffer,
               http::response<http::dynamic_body> & res)
{
    req.target("/delete");
    req.method(beast::http::verb::delete_);
    // NOTE: delete doesn't require a body
    req.body() = "Some random delete data";
    req.prepare_payload(); // set content-length based on the body
    http::write(stream, req);
    http::read(stream, buffer, res);
}


// Performs an HTTP request against httpbin.cpp.al and prints request & response
int main(int argc, char** argv)
{
    try
    {
        // Check command line arguments.
        if(argc != 2)
        {
            std::cerr <<
                "Usage: http-client-method <method> \n" <<
                "Example:\n" <<
                "    http-client-method get\n" <<
                "    http-client-method post\n";
            return EXIT_FAILURE;
        }

        for (char * c = argv[1]; *c != '\0'; c++)
            *c = static_cast<char>(std::tolower(*c));
        beast::string_view method{argv[1]};

        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve("httpbin.cpp.al", "http");

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);


        // Set up an HTTP GET request message
        http::request<http::string_body> req;
        req.set(http::field::host, "httpbin.cpp.al");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);


        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        if (method == "get")
            do_get(stream, req, buffer, res);
        else if (method == "head")
            do_head(stream, req, buffer, res);
        else if (method == "patch")
            do_patch(stream, req, buffer, res);
        else if (method == "put")
            do_put(stream, req, buffer, res);
        else if (method == "post")
            do_post(stream, req, buffer, res);
        else if (method == "delete")
            do_delete(stream, req, buffer, res);
        else
        {
            std::cerr << "Unknown method: " << method << std::endl;
            return EXIT_FAILURE;
        }

        // Write the message to standard out
        std::cout << "Request send:\n-----------------------------\n"
                  << req << std::endl;


        // Write the message to standard out
        std::cout << "\n\nResponse received:\n-----------------------------\n"
                  << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//]
