//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[http_example_get

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <string>

int main()
{
    // Normal boost::asio setup
    std::string const host = "www.example.com";
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r{ios};
    boost::asio::ip::tcp::socket sock{ios};
    boost::asio::connect(sock,
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "http"}));

    // Send HTTP request using beast
    beast::http::request<beast::http::string_body> req;
    req.method(beast::http::verb::get);
    req.target("/");
    req.version = 11;
    req.replace("Host", host + ":" +
        boost::lexical_cast<std::string>(sock.remote_endpoint().port()));
    req.replace("User-Agent", "Beast");
    req.prepare();
    beast::http::write(sock, req);

    // Receive and print HTTP response using beast
    beast::flat_buffer b;
    beast::http::response<beast::http::dynamic_body> res;
    beast::http::read(sock, b, res);
    std::cout << res << std::endl;
}

//]
