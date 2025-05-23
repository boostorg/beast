//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/test/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <thread>

namespace boost {
namespace beast {
namespace websocket {

class read1_test : public unit_test::suite
{
public:
    void
    testTimeout()
    {
        using tcp = net::ip::tcp;

        net::io_context ioc;

        // success

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            ws2.async_read(b, test::success_handler());
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            ws2.async_read(b, test::success_handler());
            test::run(ioc);
        }

        // success, timeout enabled

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(200),
                false});
            ws1.async_read(b, test::success_handler());
            ws2.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(200),
                false});
            ws1.async_read(b, test::success_handler());
            ws2.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            test::run(ioc);
        }

        // timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::fail_handler(
                beast::error::timeout));
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::fail_handler(
                beast::error::timeout));
            test::run(ioc);
        }

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            std::string res ;
            auto b = asio::dynamic_buffer(res);
            ws1.set_option(stream_base::timeout{
                    stream_base::none(),
                    std::chrono::milliseconds(200),
                    false});
            ws1.async_read(b, test::success_handler());
            ws2.async_write(net::const_buffer("Hello, world!", 13),
                            test::success_handler());
            test::run(ioc);

            BEAST_EXPECT(res == "Hello, world!");
        }


    }

    void
    testIssue2999()
    {
        net::io_context ioc;

        // Use handshake_timeout for the closing handshake,
        // which can occur in websocket::stream::async_read_some.
        stream<test::stream> ws1(ioc);
        stream<test::stream> ws2(ioc);
        test::connect(ws1.next_layer(), ws2.next_layer());
        ws1.async_handshake("test", "/", test::success_handler());
        ws2.async_accept(test::success_handler());
        test::run(ioc);

        flat_buffer b;
        ws1.set_option(stream_base::timeout{
            std::chrono::milliseconds(50),
            stream_base::none(),
            false});
        // add a close frame to the input
        ws1.next_layer().append(string_view{
            "\x88\x00", 2});
        ws1.async_read(b, test::fail_handler(
            beast::error::timeout));
        // limit the write buffer so that writing
        // the close frame will not complete during
        // the call to ioc.run_one()
        ws1.next_layer().write_size(1);
        ioc.run_one();
        ioc.restart();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
        test::run(ioc);
    }

    void
    run() override
    {
        testTimeout();
        testIssue2999();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read1);

} // websocket
} // beast
} // boost
