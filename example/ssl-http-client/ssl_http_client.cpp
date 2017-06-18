//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <string>

int main()
{
    using boost::asio::connect;
    using socket = boost::asio::ip::tcp::socket;
    using resolver = boost::asio::ip::tcp::resolver;
    using io_service = boost::asio::io_service;
    namespace ssl = boost::asio::ssl;

    // Normal boost::asio setup
    std::string const host = "localhost";
    io_service ios;
    resolver r{ios};
    socket sock{ios};
    connect(sock, r.resolve(resolver::query{host, "1007"}));

    // Perform SSL handshaking
    ssl::context ctx{ssl::context::sslv23};
    ssl::stream<socket&> stream{sock, ctx};
    stream.set_verify_mode(ssl::verify_none);
    stream.handshake(ssl::stream_base::client);

    // Send HTTP request over SSL using Beast
    beast::http::request<beast::http::string_body> req;
    req.method(beast::http::verb::get);
    req.target("/");
    req.version = 11;
    req.insert(beast::http::field::host, host + ":" +
        boost::lexical_cast<std::string>(sock.remote_endpoint().port()));
    req.insert(beast::http::field::user_agent, "Beast");
    req.prepare();
    beast::http::write(stream, req);

    // Receive and print HTTP response using Beast
    beast::flat_buffer b;
    beast::http::response<beast::http::dynamic_body> resp;
    beast::http::read(stream, b, resp);
    std::cout << resp;

    // Shut down SSL on the stream
    boost::system::error_code ec;
    stream.shutdown(ec);
    if(ec && ec != boost::asio::error::eof)
        std::cout << "error: " << ec.message();

    // Make sure everything is written before we leave main
    std::cout.flush();
}
