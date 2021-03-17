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

#include "test.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/algorithm/hex.hpp>

namespace boost {
namespace beast {
namespace websocket {

class read3_test : public websocket_test_suite
{
public:
    void
    testSuspend()
    {
        // suspend on read block
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            while(! ws.impl_->rd_block.is_locked())
                ioc.run_one();
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on release read block
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BOOST_ASSERT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on write pong
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // insert a ping
            ws.next_layer().append(string_view(
                "\x89\x00", 2));
            std::size_t count = 0;
            std::string const s = "Hello, world";
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(buffers_to_string(b.data()) == s);
                    ++count;
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    ++count;
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // Ignore ping when closing
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            // insert fragmented message with
            // a ping in between the frames.
            ws.next_layer().append(string_view(
                "\x01\x01*"
                "\x89\x00"
                "\x80\x01*", 8));
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(buffers_to_string(b.data()) == "**");
                    BEAST_EXPECT(++count == 1);
                    b.consume(b.size());
                    ws.async_read(b,
                        [&](error_code ec, std::size_t)
                        {
                            if(ec != net::error::operation_aborted)
                                BOOST_THROW_EXCEPTION(
                                    system_error{ec});
                            BEAST_EXPECT(++count == 3);
                        });
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // See if we are already closing
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            // insert fragmented message with
            // a close in between the frames.
            ws.next_layer().append(string_view(
                "\x01\x01*"
                "\x88\x00"
                "\x80\x01*", 8));
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 2);
        });
    }

    void
    testParseFrame()
    {
        auto const bad =
            [&](string_view s)
            {
                echo_server es{log};
                net::io_context ioc;
                stream<test::stream> ws{ioc};
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/");
                ws.next_layer().append(s);
                error_code ec;
                multi_buffer b;
                ws.read(b, ec);
                BEAST_EXPECT(ec);
            };

        // chopped frame header
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x81\x7e\x01");
            std::size_t count = 0;
            std::string const s(257, '*');
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(buffers_to_string(b.data()) == s);
                });
            ioc.run_one();
            es.stream().write_some(
                net::buffer("\x01" + s));
            ioc.run();
            BEAST_EXPECT(count == 1);
        }

        // new data frame when continuation expected
        bad("\x01\x01*" "\x81\x01*");

        // reserved bits not cleared
        bad("\xb1\x01*");
        bad("\xc1\x01*");
        bad("\xd1\x01*");

        // continuation without an active message
        bad("\x80\x01*");

        // reserved bits not cleared (cont)
        bad("\x01\x01*" "\xb0\x01*");
        bad("\x01\x01*" "\xc0\x01*");
        bad("\x01\x01*" "\xd0\x01*");

        // reserved opcode
        bad("\x83\x01*");

        // fragmented control message
        bad("\x09\x01*");

        // invalid length for control message
        bad("\x89\x7e\x01\x01");

        // reserved bits not cleared (control)
        bad("\xb9\x01*");
        bad("\xc9\x01*");
        bad("\xd9\x01*");

        // unmasked frame from client
        {
            echo_server es{log, kind::async_client};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            ws.next_layer().append(
                "\x81\x01*");
            error_code ec;
            multi_buffer b;
            ws.read(b, ec);
            BEAST_EXPECT(ec);
        }

        // masked frame from server
        bad("\x81\x80\xff\xff\xff\xff");

        // chopped control frame payload
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x89\x02*");
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(buffers_to_string(b.data()) == "**");
                });
            ioc.run_one();
            es.stream().write_some(
                net::buffer(
                    "*" "\x81\x02**"));
            ioc.run();
            BEAST_EXPECT(count == 1);
        }

        // length not canonical
        bad(string_view("\x81\x7e\x00\x7d", 4));
        bad(string_view("\x81\x7f\x00\x00\x00\x00\x00\x00\xff\xff", 10));
    }

    void
    testIssue802()
    {
        for(std::size_t i = 0; i < 100; ++i)
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // too-big message frame indicates payload of 2^64-1
            net::write(ws.next_layer(), sbuf(
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
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        ws.write(sbuf("Hello, world!"));
        char buf[4];
        net::mutable_buffer b{buf, 0};
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
        multi_buffer b;
        net::io_context ioc;
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
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            net::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            net::io_context ioc;
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
            net::write(wsc.next_layer(), sbuf(
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
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            net::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            net::io_context ioc;
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
            net::write(wsc.next_layer(), sbuf(
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
            net::io_context ioc;
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
            net::write(wsc.next_layer(), sbuf(
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
            net::io_context ioc;
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
            net::write(wsc.next_layer(), sbuf(
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

    /*  Bishop Fox Hybrid Assessment issue 2

        Happens with permessage-deflate enabled,
        and a deflate block with the BFINAL bit set
        is encountered in a compressed payload.
    */
    void
    testIssue1630()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

        net::io_context ioc;
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

        const std::vector<std::string> lines = {
            "c12d"
            "aa56ca4b4dcf2fc94c2c494d09c9cc4dcd2f2d51b23235303"
            "0d0512aa92c4855b2522a4fcd49cecf4d55aa0500",

            "c17e0b89"
            "d45adb8edb4892fd15a11e175baebc5fb8f04349a5c2f6a2dd"
            "30fa3698d91934f21229b1742d92525d0cfffb1c4ab6dba2c7"
            "d3e8925f067e289399c9c88c3811e704a9771771939f2eaa77"
            "177b6ada7ab3bea8c4a75969b35e53c2432efefb624bd4fcf6"
            "e106a6b517d5ffbfbba83366699a46ca21a4accdf8d614afb8"
            "4f7962b31c3b3de11e8b33b5a9a9b7dde1f9ef3e3e3eacdb07"
            "6a30dce62d2ef7afd9df9bbfaf37af2f47d209a5ad36527dfe"
            "dfef7e187df7568dd8abc3bf7e6efbfad737c44414265891b2"
            "302c6a923258ed420c2c598ab19fd7bd66a3c382f0ba4e74b9"
            "ac3b3a5eadda3a5fb6b40aebae4ed5e82f6f7e1afdd77164d6"
            "6c76db6afccb0f37df4f476197ebcd685f67daf4a3abd7c71b"
            "dc702e46bfdcbcbdfaf9fb9fae7efcf9edd54fd7bfbebd1d71"
            "ce8fe6d2eb0f7bd6ea1537f29532af38374703f4d8adc2b6e2"
            "a35db3ae6aea4ab50d4d58b555d36d2fe7b9c178d5b64dba3c"
            "d8ba5cd29e96c7954d97b0ee60fbeb8fc7530e8fc75636db5d"
            "7ba51ce27f254e06d9e8ede4cd2f57fdc8f17e5975c725ab7a"
            "8d68ade83567ffb36ba95ec7b0ce85d26b7e9c98705967e0ab"
            "1223de7b60241897c21825d9e96e3e3809211fcd376df7e572"
            "71b25cf82f963bad06cb7b075cae768fc7ab963a442a0096fb"
            "8f51ad7375f0da8790af3f1fec01b02b4d98557f91e3d5eff7"
            "b60fb9fabfc9f7e56fbf88e436fcfbbfbef9a1b97e93fef7e9"
            "bb0fbea9d7336ab64dbd465ce6e1526833babea9aca8a6934a"
            "898a4d2ac9aaa9ad1caf8cadae27159f56b7aa62bc9fa35835"
            "beaea4abb8aeacabec4d2554757353e9497f67ccab9bdbca4c"
            "2b31ae24ee4ffbfb521fb17680dd57b1e6e122862008fb4780"
            "3b780d39fe4c270014a37987a85f5d3d3c3cbc7aa08869af36"
            "cdec8a1eb7d40002ebaebdfa1d925721b6489875beece1f1e7"
            "f188edfefad65d79f609731f818a433494ffd588b0a3dd720b"
            "f49d0ee23425f60f9c6d36b3cb8656f18b919456a352375fdc"
            "072016fff2e668bbacff5310fea91cfd87231c04d0d0be3e92"
            "0f7ffffe1f208403b1fcf8264aadb292969c0d146de1414823"
            "bc294ad8a055c2d27558f55cd251dbfdb8d9ac784f55a14135"
            "afb728ea3ddb1c1ff6f6da076e98d64ca9985de43107e5b835"
            "cc5ab29a978895f02888a0031f5ea8c5836c1f1e141701035d"
            "0370f49cf78f93cd62a71d627c4295ef4f3670e4c98f5b50c9"
            "0ac548276e9dd1c111272adc32e29a7b7bb0f4d9169e42fd1c"
            "b5dfdccffeec0e7eb7e86ce48931ee7524096b8a608c65a58b"
            "2f52657e6a71beb2dd7e3d8b0fdb975b14644320c902f74a38"
            "a1098c2ca4734130119233a7161f9f776cbeb7baac4f4cbebb"
            "58d4ebfe798732dec7b4a937cd7109ccaf7368f2e141b89ec4"
            "20d46d9a4eb91f73916ff9d4fab1b35375e3fc58c86bcca375"
            "884becb3ea9a1d7d84d79b9f592615954b919b42e48822b618"
            "12e5929996e533781db6d13bebe3d91b0a904defffa46ba4cd"
            "3ac910e00d6645b1c5150a0e7f1c1516f4a96bb6cbfc3c7f7e"
            "6e827b79304a61521003d41113328a2b0b81a62d67646ca401"
            "e09c7a5e6dd64f337b864529b313dc050f9f966088bc0c3951"
            "04e845317c6071bf17cbd94c3fabfbf8729354104598d34691"
            "e3c567e37872d1b3e2b904f24e4d2e6a733733e6697f777f86"
            "499962cc493a29549192219f19591193b57036f95393cf493c"
            "74eb559ae99762dc8ebd4cc08b08d7651288297633f688a7b9"
            "e5663266e6eb182f361a535c2edcbb88248c5681839400f482"
            "c8d17d6b8c6716b1536f6dd42c3b168939e1c99bbe801b00e2"
            "d4336566eeb6cf0f4e29f352d7e46be739bfa1a88c33e499b8"
            "71c50aeb43361319b4fbba6b2818998d2849ea4c5a0ba94a22"
            "1e5dc9dc0755beb96ba2964672a6858e466450182a3017e857"
            "94c93c8501012d5a24d1d3c2add9fc0c9c221a21e410bd96de"
            "71278b6220402772eeb73030a9bbd67bbf8ffbae9c51ff5930"
            "3e5a9f3522ae72629e64e61c65d519c1e3a0c8b56eb369453b"
            "6767705c4abd28c8922b9e2c7a4169a838c44f4a6e751e424e"
            "37dbc7edfea97ef467985485832014051c0e96ad0b8af362b2"
            "d301955d0e6835d7b65d28f594d54b417e339e06ba4dd7b68c"
            "7b013365137d3b09ce84588cbefd77f90f81a39db45c1a280c"
            "af28168d7aec3c017f4290fad6202f59d9ac10744bc92b23b4"
            "48d2fbc4a3e1cacba1c4296c7fcf3b7b87047c7930b406cbe5"
            "98b343a58b05a7cb7da3202006a0a96c3e35691528c1773570"
            "f772932810c66b2158ce647dca3c6b6b74b489f364241b809c"
            "b4cf774f51247506e520a1248bdeb8a0217f0bca88a324c869"
            "b2c917af0654cec0878b8de816f91c9433a3bd889241b7a522"
            "7d48067a5cf304e63043c5cc969daee7a1d6852fcf08262a07"
            "a0e9a357c53a5cf908a7925688a73176c0ac824c575ab562cf"
            "2fcdac08ed2d85e7f67a72dd876e22d9584b3b9e5e6be5c653"
            "fff5ccf2941c73c878e625a9a050ce0b1789071251f003ecbe"
            "ad7acc2607cd0ce3096c0eba42958d29121a24c1581103f558"
            "3f85c74db1078fbd58e51470abe326f62f0fa389468bec3cf4"
            "0d2a2e2b03c841683eeefd2acfdbc5cb4d3274c4c968cbd02a"
            "331e208a93b1c5eac88c096578c8a7e2b6e97ebedbbdb8b02a"
            "88a75b757d33d537199a652c9974e3341d5fc789981816be1e"
            "7e212cb7058d23682ea0fe180a0cd14f5ea3f045ffcdd50311"
            "4ba0188636d62aab6011ea2665053f81cef5c033cd12cde5c2"
            "b9fba7fa0c6225a9241a13698dcb21436043c443cb3bef9199"
            "210f4df29d707b126755b98263f10477a239f5266654209724"
            "8f925289ce9e9a5cd74a2f964d58752f8dbf80c188fe546681"
            "7ee85a8cf34d0c1348f99b7130ec76f26f9a4742b045d1ce41"
            "8d17e10caa15491315d311acf7cde3cfd0bd5b6da1df934600"
            "7c289c02aa80404baf50ec079e69d5bcbddbda993fa7fea36b"
            "04c823e34c5a2b506fb20805984bac4832036125d966e15436"
            "fbbb33841513252ac155ce905546950875ac4c425b17b91476"
            "607289e6711e9ae6a99ca191218d208799d5c6270569ee9383"
            "2ce7b0cb89cb61c9118fbb0d7f94f366255f6ed2164382c9e8"
            "988f321754b852285be50bca1e94d3e01d499c2f561b41ebd9"
            "19b124889092b8628960d4489599774940270819fd5024dfab"
            "f5ec7e1d9e6674465f2e1d8a93835c40f45441175838b25905"
            "10ab4b5c0ca8fcfea19ddfdd6f1fdbc51926b5464a90029127"
            "4a56b390827096347a0f2a5a0c144b114d6abc9c697d8660a1"
            "c8c1faa67f9b16139a0e835e24592fac8a6877fda062dd896d"
            "bd601dd9bb33d23240059afe2da843d7e81141ed83d0c11601"
            "0cbb32f0abdcb7165ae0419da177213d954465879eb031a0de"
            "591b127225a15a7b2e07c5a7f8c596a7d6dddbcd194c608114"
            "9990fda57f7f844e276b868c8134b028da83b4944fbe707e9f"
            "f7ea8c1cd1c1a39d82ce4de0581c583ace7316ae6f97550983"
            "50baa55c747c2f5012cfa0580f42476ea28e73c67d123c2817"
            "4c80f48186189e92b93685c592671bcef06cdf2ff9e055cc85"
            "65e572715694e2b34d211b68fc539b3c3eecedbeccebd49d61"
            "326a30adb7c5176505796f380a814adeebe8859283f765ebf6"
            "fe4975bbe717cb3a06761e4fc1202938399e4c7a3a17604fe2"
            "d75adc9af4755aef9b0f850249de8012c8f9482c6a8f5296b2"
            "0600bf79bf6cd14342ab25d57f9d504673210d499d6cf468f2"
            "06fdf203bcb65c473b1367502c542ba11985b6d2090a3f875e"
            "6399e0ace3e486edf25eb325378b8d6fcf78298cd3f5e2059a"
            "3514c87b5462c8e6ec8230a48df483d725899ab6d372c5ba33"
            "8aa4646073741194d0b840515869235704da29e0773640dc02"
            "4ddcbc7f63f467317e9892364daed7b3fe63d3275495b06c69"
            "b0be6e7ffb6cf261c67b3c7517fb5f47c47ed5bbcf16b0cf77"
            "d27f55dac565ddce87d3f8605a4bed71e042ab83ae61217870"
            "a276c5230e681b7d08a06150c71f7c5dfba30f6616090ca5a4"
            "a2ce8c65237936a55810936442b903296cb61f7e2cf2ee6245"
            "b90e38ffecb8b95d7b489d7ab60e4bb8e38b81e3297eeb3efe"
            "7c45b2cf67539e1d52b09dc75db3be78fffed36f5656edece2"
            "fd3f01",

            "c17e0273"
            "ec9ac16edc2010865f25f27d24981d83c9a9af02363e454dd4"
            "34957ac8bbf7f7aaf242b5ad41e94add6a6e7bd8f9c706ccfc"
            "03df1fa996b797ed5e76b809d2f20c1fd74cb48c1f205af68b"
            "fe22f656944b13e1523cc7ade193336f10ae0cde47a09a7ff2"
            "beff25bebe5e455a3602035bc9b7bbbcf0c7e3630af0270c6d"
            "1831320ff356521f31d75fb051c34ea214854fdbe8109c183e"
            "065eafc76d0bfb71c96b7c7bfafa1056b45330fe245698b02b"
            "190a1c3ca1a86e9534e09b2a65609a27582e6ba7bef4455c95"
            "7e4623c56174e4c312092ec711f6c04c0b7a589fd282df4b29"
            "33f93304c0a633fd25ae4acfe13439c333a195748436dc52b2"
            "2151f0d6c246e76cf35cc89cb0849d4ca3e3bef4455c95dea5"
            "1ccd8a21b71213899984429e2239981b19674e4bf0958c2051"
            "4047df9b7e8fabd28f2850c64c4cc9b1258199a1ed66845ce4"
            "2478104e594a19d4357382ddea9cfb22ae7e7b37a113f2914c"
            "9013d69f7714d36cb01404b60e7b9749b50ca38f32e27bdffe"
            "1257a54f92382583a50e1b47023749e8eb85222c50f228f96b"
            "0e850cf624f658aa635ff64b5895dc66b1711164837d205887"
            "89e2cc81e05de39245d82c5c816ced10dbb59af25701b2df54"
            "919b9272ed085a3fe2760745aca0d6b602f6fcf9e9fb5d16b1"
            "a1f4f6acd49a526b4aad29b5a6d49a526b4aad29b5a6d49a52"
            "6b4aad29b5a6d49a526b4aad29b5a6d49a526b4aad29b5a6d4"
            "9a526b4aad29b5a6d49a526bff27b566abb57f7ead961be2a1"
            "e37e631bb09f36f89879183a3e925db8052728840f4fab77e1"
            "1650a0103eecd62ec20dfcc5d0d106ecc22d544d217c78bcb0"
            "0bb7400b85f0e189e02edc02e214c287e7e9efcdf0e52f14e2"
            "0f00",

            "c145"
            "ecda310d000000c320ffae67a1f7820e680b911a53636a4c8d"
            "a93135a6c6d4981a53636a4c8da93135a6c6d4981a53636a4c"
            "8da93135a6c6d4981a53636a4c8da9f1a71a0f",
        };

        for (const auto & line : lines) {
            std::vector<uint8_t> out;
            boost::algorithm::unhex(line.begin(), line.end(), std::back_inserter(out));
            net::write(wss.next_layer(), net::const_buffer(out.data(), out.size()));
            multi_buffer buffer;
            error_code ec;
            wsc.async_read(buffer, [&ec](error_code ec_, std::size_t) { ec = ec_; });
            ioc.run();
            ioc.restart();
            BEAST_EXPECTS(!ec, ec.message());
        }
    }

    void
    testMoveOnly()
    {
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_read_some(
            net::mutable_buffer{},
            move_only_handler{});
    }

    struct copyable_handler
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    void
    testAsioHandlerInvoke()
    {
        // make sure things compile, also can set a
        // breakpoint in asio_handler_invoke to make sure
        // it is instantiated.
        {
            net::io_context ioc;
            net::strand<
                net::io_context::executor_type> s(
                    ioc.get_executor());
            stream<test::stream> ws{ioc};
            flat_buffer b;
            ws.async_read(b, net::bind_executor(
            s, copyable_handler{}));
        }
    }

    void
    run() override
    {
        testParseFrame();
        testIssue802();
        testIssue807();
        testIssue954();
        testIssue1630();
        testIssueBF1();
        testIssueBF2();
        testMoveOnly();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read3);

} // websocket
} // beast
} // boost
