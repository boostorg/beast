//
// Copyright (w) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include "test.hpp"

#include <boost/asio/write.hpp>

#include <boost/beast/core/buffers_to_string.hpp>

namespace boost {
namespace beast {
namespace websocket {

class read2_test : public websocket_test_suite
{
public:
    void
    testIssue802()
    {
        for(std::size_t i = 0; i < 100; ++i)
        {
            echo_server es{log, kind::async};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // too-big message frame indicates payload of 2^64-1
            boost::asio::write(ws.next_layer(), sbuf(
                "\x81\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"));
            multi_buffer b;
            error_code ec;
            ws.read(b, ec);
            BEAST_EXPECT(ec == error::closed);
            BEAST_EXPECT(ws.reason().code == 1009);
        }
    }

    void
    testIssue807()
    {
        echo_server es{log};
        boost::asio::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        ws.write(sbuf("Hello, world!"));
        char buf[4];
        boost::asio::mutable_buffer b{buf, 0};
        auto const n = ws.read_some(b);
        BEAST_EXPECT(n == 0);
    }

    /*
        When the internal read buffer contains a control frame and
        stream::async_read_some is called, it is possible for the control
        callback to be invoked on the caller's stack instead of through
        the executor associated with the final completion handler.
    */
    void
    testIssue954()
    {
        echo_server es{log};
        boost::asio::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        // message followed by ping
        ws.next_layer().append({
            "\x81\x00"
            "\x89\x00",
            4});
        bool called_cb = false;
        bool called_handler = false;
        ws.control_callback(
            [&called_cb](frame_type, string_view)
            {
                called_cb = true;
            });
        multi_buffer b;
        ws.async_read(b,
            [&](error_code, std::size_t)
            {
                called_handler = true;
            });
        BEAST_EXPECT(! called_cb);
        BEAST_EXPECT(! called_handler);
        ioc.run();
        BEAST_EXPECT(! called_cb);
        BEAST_EXPECT(called_handler);
        ws.async_read(b,
            [&](error_code, std::size_t)
            {
            });
        BEAST_EXPECT(! called_cb);
    }

    /*  Bishop Fox Hybrid Assessment issue 1

        Happens with permessage-deflate enabled and a
        compressed frame with the FIN bit set ends with an
        invalid prefix.
    */
    void
    testIssueBF1()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

        // read
#if 0
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            boost::asio::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            boost::asio::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // invalid 1-byte deflate block in frame
            boost::asio::write(wsc.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
            error_code ec;
            multi_buffer b;
            wss.read(b, ec);
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }

        // async read
#if 0
        {
            echo_server es{log, kind::async};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            boost::asio::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            boost::asio::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // invalid 1-byte deflate block in frame
            boost::asio::write(wsc.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
            error_code ec;
            flat_buffer b;
            wss.async_read(b,
                [&ec](error_code ec_, std::size_t){ ec = ec_; });
            ioc.run();
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }
    }

    /*  Bishop Fox Hybrid Assessment issue 2

        Happens with permessage-deflate enabled,
        and a deflate block with the BFINAL bit set
        is encountered in a compressed payload.
    */
    void
    testIssueBF2()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

        // read
        {
            boost::asio::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // contains a deflate block with BFINAL set
            boost::asio::write(wsc.next_layer(), sbuf(
                "\xc1\xf8\xd1\xe4\xcc\x3e\xda\xe4\xcc\x3e"
                "\x2b\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\x1e"
                "\x36\x3e\x35\xae\x4f\x54\x18\xae\x4f\x7b"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\xe4"
                "\x28\x74\x52\x8e\x05\x74\x52\xa1\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\x36\x3e"
                "\xd1\xec\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e"));
            error_code ec;
            flat_buffer b;
            wss.read(b, ec);
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }

        // async read
        {
            boost::asio::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // contains a deflate block with BFINAL set
            boost::asio::write(wsc.next_layer(), sbuf(
                "\xc1\xf8\xd1\xe4\xcc\x3e\xda\xe4\xcc\x3e"
                "\x2b\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\x1e"
                "\x36\x3e\x35\xae\x4f\x54\x18\xae\x4f\x7b"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\xe4"
                "\x28\x74\x52\x8e\x05\x74\x52\xa1\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\x36\x3e"
                "\xd1\xec\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e"));
            error_code ec;
            flat_buffer b;
            wss.async_read(b,
                [&ec](error_code ec_, std::size_t){ ec = ec_; });
            ioc.run();
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }
    }

    void
    run() override
    {
        testIssue802();
        testIssue807();
        testIssue954();
        testIssueBF1();
        testIssueBF2();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read2);

} // websocket
} // beast
} // boost
