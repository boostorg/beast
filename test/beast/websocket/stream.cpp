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

namespace boost {
namespace beast {
namespace websocket {

class stream_test : public websocket_test_suite
{
public:
    void
    testOptions()
    {
        stream<test::stream> ws(ios_);
        ws.auto_fragment(true);
        ws.write_buffer_size(2048);
        ws.binary(false);
        ws.read_message_max(1 * 1024 * 1024);
        try
        {
            ws.write_buffer_size(7);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    //--------------------------------------------------------------------------

    template<class Wrap>
    void
    doTestStream(Wrap const& w,
        permessage_deflate const& pmd)
    {
        using boost::asio::buffer;

        // send empty message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.text(true);
            w.write(ws, boost::asio::null_buffers{});
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(b.size() == 0);
        });

        // send message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // read_some
        doTest(pmd, [&](ws_type& ws)
        {
            w.write(ws, sbuf("Hello"));
            char buf[10];
            auto const bytes_written =
                w.read_some(ws, buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            buf[bytes_written] = 0;
            BEAST_EXPECT(
                string_view(buf).substr(0, bytes_written) ==
                string_view("Hello").substr(0, bytes_written));
        });

        // close, no payload
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
        });

        // close with code
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, close_code::going_away);
        });

        // send ping and message
        doTest(pmd, [&](ws_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "");
                };
            ws.control_callback(cb);
            w.ping(ws, "");
            ws.binary(true);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(ws.got_binary());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // send ping and fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "payload");
                };
            ws.control_callback(cb);
            ws.ping("payload");
            w.write_some(ws, false, sbuf("Hello, "));
            w.write_some(ws, false, sbuf(""));
            w.write_some(ws, true, sbuf("World!"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(to_string(b.data()) == "Hello, World!");
            ws.control_callback();
        });

        // send pong
        doTest(pmd, [&](ws_type& ws)
        {
            w.pong(ws, "");
        });

        // send auto fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(true);
            ws.write_buffer_size(8);
            w.write(ws, sbuf("Now is the time for all good men"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == "Now is the time for all good men");
        });

        // send message with write buffer limit
        doTest(pmd, [&](ws_type& ws)
        {
            std::string s(2000, '*');
            ws.write_buffer_size(1200);
            w.write(ws, buffer(s.data(), s.size()));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // unexpected cont
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x80, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(w, ws, close_code::protocol_error);
        });

        // invalid fixed frame header
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x8f, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(w, ws, close_code::protocol_error);
        });

        if(! pmd.client_enable)
        {
            // expected cont
            doTest(pmd, [&](ws_type& ws)
            {
                w.write_some(ws, false, boost::asio::null_buffers{});
                w.write_raw(ws, cbuf(
                    0x81, 0x80, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(w, ws, close_code::protocol_error);
            });

            // message size above 2^64
            doTest(pmd, [&](ws_type& ws)
            {
                w.write_some(ws, false, cbuf(0x00));
                w.write_raw(ws, cbuf(
                    0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(w, ws, close_code::too_big);
            });

            /*
            // message size exceeds max
            doTest(pmd, [&](ws_type& ws)
            {
                // VFALCO This was never implemented correctly
                ws.read_message_max(1);
                w.write(ws, cbuf(0x81, 0x02, '*', '*'));
                doCloseTest(w, ws, close_code::too_big);
            });
            */
        }

        // receive ping
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x89, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::ping);
                invoked = true;
            };
            ws.control_callback(cb);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // receive ping
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::close);
                invoked = true;
            };
            ws.control_callback(cb);
            w.write(ws, sbuf("Hello"));
            doCloseTest(w, ws, close_code::none);
        });

        // receive bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            doFailTest(w, ws, error::failed);
        });

        // receive bad close
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x02, 0x03, 0xed));
            doFailTest(w, ws, error::failed);
        });
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream>, boost::asio::io_service&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream>>::value);

        BOOST_STATIC_ASSERT(std::is_move_assignable<
            stream<test::stream>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream&>, test::stream&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream&>>::value);

        BOOST_STATIC_ASSERT(! std::is_move_assignable<
            stream<test::stream&>>::value);

        log << "sizeof(websocket::stream) == " <<
            sizeof(websocket::stream<test::stream&>) << std::endl;

        testOptions();

        auto const testStream =
            [this](permessage_deflate const& pmd)
            {
                doTestStream(SyncClient{}, pmd);

                yield_to(
                    [&](yield_context yield)
                    {
                        doTestStream(AsyncClient{yield}, pmd);
                    });
            };

        permessage_deflate pmd;

        pmd.client_enable = false;
        pmd.server_enable = false;
        testStream(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = false;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        testStream(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = true;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        testStream(pmd);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
