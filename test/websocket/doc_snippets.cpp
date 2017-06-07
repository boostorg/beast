//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

using namespace beast;

//[ws_snippet_1
#include <beast/websocket.hpp>
using namespace beast::websocket;
//]

namespace doc_ws_snippets {

void fxx() {

boost::asio::io_service ios;
boost::asio::io_service::work work{ios};
std::thread t{[&](){ ios.run(); }};
error_code ec;
boost::asio::ip::tcp::socket sock{ios};

{
//[ws_snippet_2
    stream<boost::asio::ip::tcp::socket> ws{ios};
//]
}

{
//[ws_snippet_3
    stream<boost::asio::ip::tcp::socket> ws{std::move(sock)};
//]
}

{
//[ws_snippet_4
    stream<boost::asio::ip::tcp::socket&> ws{sock};
//]

//[ws_snippet_5
    ws.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send);
//]
}

{
//[ws_snippet_6
    std::string const host = "mywebapp.com";
    boost::asio::ip::tcp::resolver r{ios};
    stream<boost::asio::ip::tcp::socket> ws{ios};
    boost::asio::connect(ws.next_layer(),
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "ws"}));
//]
}

{
//[ws_snippet_7
    boost::asio::ip::tcp::acceptor acceptor{ios};
    stream<boost::asio::ip::tcp::socket> ws{acceptor.get_io_service()};
    acceptor.accept(ws.next_layer());
//]
}

} // fxx()

} // doc_ws_snippets
