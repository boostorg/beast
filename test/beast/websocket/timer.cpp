
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

class timer_test
    : public websocket_test_suite
{
public:
    using tcp = boost::asio::ip::tcp;

    static
    void
    connect(
        stream<tcp::socket>& ws1,
        stream<tcp::socket>& ws2)
    {
        struct handler
        {
            void
            operator()(error_code ec)
            {
                BEAST_EXPECTS(! ec, ec.message());
            }
        };

        tcp::acceptor a(ws1.get_executor().context());
        auto ep = tcp::endpoint(
            net::ip::make_address_v4("127.0.0.1"), 0);
        a.open(ep.protocol());
        a.set_option(
            net::socket_base::reuse_address(true));
        a.bind(ep);
        a.listen(0);
        ep = a.local_endpoint();
        a.async_accept(ws2.next_layer(), handler{});
        ws1.next_layer().async_connect(ep, handler{});
        for(;;)
        {
            if( ws1.get_executor().context().run_one() +
                ws2.get_executor().context().run_one() == 0)
            {
                ws1.get_executor().context().restart();
                ws2.get_executor().context().restart();
                break;
            }
        }
        BEAST_EXPECT(
            ws1.next_layer().remote_endpoint() ==
            ws2.next_layer().local_endpoint());
        BEAST_EXPECT(
            ws2.next_layer().remote_endpoint() ==
            ws1.next_layer().local_endpoint());
        ws2.async_accept(handler{});
        ws1.async_handshake("test", "/", handler{});
        for(;;)
        {
            if( ws1.get_executor().context().run_one() +
                ws2.get_executor().context().run_one() == 0)
            {
                ws1.get_executor().context().restart();
                ws2.get_executor().context().restart();
                break;
            }
        }
        BEAST_EXPECT(ws1.is_open());
        BEAST_EXPECT(ws2.is_open());
        BEAST_EXPECT(! ws1.get_executor().context().stopped());
        BEAST_EXPECT(! ws2.get_executor().context().stopped());
    }

#if 0
    void
    testRead0()
    {
        echo_server es(log, kind::async);

        net::io_context ioc;
        stream<test::stream> ws(ioc);
        ws.next_layer().connect(es.stream());

        ws.handshake("localhost", "/");
        flat_buffer b;
        ws.async_read(b,
            [&](error_code ec, std::size_t)
            {
                log << ec.message() << "\n";
            });
        ioc.run();
    }
#endif

    void
    testRead()
    {
        net::io_context ioc;
        stream<tcp::socket> ws1(ioc);
        stream<tcp::socket> ws2(ioc);
        connect(ws1, ws2);
        flat_buffer b;

        ws2.async_read(b,
            [&](error_code ec, std::size_t)
            {
                log << "ws1.async_read: " << ec.message() << "\n";
            });
        ioc.run();
    }

    void
    run() override
    {
        testRead();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,timer);

} // websocket
} // beast
} // boost
