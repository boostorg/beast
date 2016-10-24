//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_async_echo_server.hpp"
#include "websocket_sync_echo_server.hpp"
#include <beast/test/sig_wait.hpp>
#include <iostream>

int main()
{
    using namespace beast::websocket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;

    try
    {
        beast::error_code ec;
    	async_echo_server s1{nullptr, 1};
        s1.open(endpoint_type{
            address_type::from_string("127.0.0.1"), 6000 }, ec);
        s1.set_option(read_message_max{64 * 1024 * 1024});
        s1.set_option(auto_fragment{false});
        //s1.set_option(write_buffer_size{64 * 1024});

    	beast::websocket::sync_echo_server s2(&std::cout, endpoint_type{
	        address_type::from_string("127.0.0.1"), 6001 });
        s2.set_option(read_message_max{64 * 1024 * 1024});

    	beast::test::sig_wait();
    }
    catch(std::exception const& e)
    {
    	std::cout << "Error: " << e.what() << std::endl;
    }
}
