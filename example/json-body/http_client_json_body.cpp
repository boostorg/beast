//
// Copyright (c) 2022 Seth Heeren(sgheeren at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, synchronous, POST with json_body
//
//------------------------------------------------------------------------------

//[example_http_client_json_body

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "json_body.hpp"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using beast::contrib::json_body;

// Performs a json_body HTTP POST and prints the response
int main()
{
    try
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        tcp::socket stream(ioc);
        //beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve("httpbin.org", "80");

        // Make the connection on the IP address we get from a lookup
        net::connect(stream, results);
        //stream.connect(results);

        // Set up an HTTP POST request message with chosen payload type
        http::request<json_body> req{http::verb::post, "/post?param1=foo%26bar&no_value", 11};
        req.set(http::field::host, "httpbin.org");
        //req.set(http::field::connection, "close");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        req.body() = boost::json::value{
            {"foo", {1, 2, 3, 4, 5}},
            {"bar",
             {
                 {"nested-foo",
                  {
                      {"one", 11},
                      {"two", 22},
                      {"three", 33},
                      {"four", 44},
                  }},
             }},
        };
        req.set(http::field::content_type, "application/json");
        req.prepare_payload();

        std::cout << "SENDING\n=======\n" << req << "\n======\n";
        // Send the HTTP request to the remote host
        http::write(stream, req);
        //stream.shutdown(tcp::socket::shutdown_send);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<json_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Inspect the response and write diagnostic information
        std::cout << "Returned response headers\n";
        std::cout << "=========================\n";
        std::cout << res.base() << "\n";

        std::cout << "Parsed response body\n";
        std::cout << "====================\n";
        std::cout << res.body() << "\n";

        {
            std::cout << "\n"
                      << "Parsed response checks\n";
            std::cout << "======================\n" << std::boolalpha;
            auto const& args = res.body().at("args");
            auto const& data = res.body().at("data").as_string();
            auto const& json = res.body().at("json");

            auto const& expected_args = boost::json::value{
                {"param1", "foo&bar"},
                {"no_value", ""},
            };
            auto const& expected_json = req.body();
            auto const& expected_data = serialize(expected_json);

            auto const args_ok = (args == expected_args)?"PASS":"FAIL";
            auto const json_ok = (json == expected_json)?"PASS":"FAIL";
            auto const data_ok = (data == expected_data)?"PASS":"FAIL";

            std::cout << "Args: " << args_ok << " " << args << "\n";
            std::cout << "Data: " << data_ok << " " << data << "\n";
            std::cout << "JSON: " << json_ok << " " << json << "\n";
        }

        // Gracefully close the socket
        stream.shutdown(tcp::socket::shutdown_both);
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//]
