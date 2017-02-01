//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_async_echo_server.hpp"
#include "websocket_sync_echo_server.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>

/// Block until SIGINT or SIGTERM is received.
void
sig_wait()
{
    boost::asio::io_service ios;
    boost::asio::signal_set signals(
        ios, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code const&, int)
        {
        });
    ios.run();
}

int main()
{
    using namespace beast::websocket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;

    beast::error_code ec;

    permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    pmd.compLevel = 3;

    websocket::async_echo_server s1{&std::cout, 1};
    s1.set_option(read_message_max{64 * 1024 * 1024});
    s1.set_option(auto_fragment{false});
    s1.set_option(pmd);
    s1.open(endpoint_type{
        address_type::from_string("127.0.0.1"), 6000 }, ec);

    websocket::sync_echo_server s2{&std::cout};
    s2.set_option(read_message_max{64 * 1024 * 1024});
    s2.set_option(auto_fragment{false});
    s2.set_option(pmd);
    s2.open(endpoint_type{
        address_type::from_string("127.0.0.1"), 6001 }, ec);

    sig_wait();
}
