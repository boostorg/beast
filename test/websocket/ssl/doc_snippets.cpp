//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

using namespace beast;
using namespace beast::websocket;

//[wss_snippet_1
#include <beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
//]

namespace doc_wss_snippets {

void fxx() {

boost::asio::io_service ios;
boost::asio::io_service::work work{ios};
std::thread t{[&](){ ios.run(); }};
error_code ec;
boost::asio::ip::tcp::socket sock{ios};

{
//[wss_snippet_2
    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
    stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> wss{ios, ctx};
//]
}

{
//[wss_snippet_3
    boost::asio::ip::tcp::endpoint ep;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
    stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ws{ios, ctx};

    // connect the underlying TCP/IP socket
    ws.next_layer().next_layer().connect(ep);

    // perform SSL handshake
    ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

    // perform WebSocket handshake
    ws.handshake("localhost", "/");
//]
}

} // fxx()

} // doc_wss_snippets
