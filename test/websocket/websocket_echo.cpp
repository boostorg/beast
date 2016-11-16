//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_async_echo_server.hpp"
#include "websocket_sync_echo_server.hpp"
#include <beast/test/sig_wait.hpp>

int main()
{
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;

    beast::websocket::async_echo_server s1(true, endpoint_type{
        address_type::from_string("127.0.0.1"), 6000 }, 1);

    beast::websocket::sync_echo_server s2(true, endpoint_type{
        address_type::from_string("127.0.0.1"), 6001 });

    beast::test::sig_wait();
}
