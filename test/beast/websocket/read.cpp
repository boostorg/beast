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

class read_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doReadTest(
        Wrap const& w,
        ws_type& ws,
        close_code code)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != error::closed)
                throw;
            BEAST_EXPECT(
                ws.reason().code == code);
        }
    }

    template<class Wrap>
    void
    doFailTest(
        Wrap const& w,
        ws_type& ws,
        error_code ev)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != ev)
                throw;
        }
    }

    template<class Wrap>
    void
    doTestRead(Wrap const& w)
    {
        using boost::asio::buffer;

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // already closed
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.close({});
            try
            {
                multi_buffer b;
                w.read(ws, b);
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // empty, fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append(
                string_view(
                    "\x01\x00" "\x80\x00", 4));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(b.size() == 0);
        });

        // two part message
        // this triggers "fill the read buffer first"
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, sbuf(
                "\x01\x81\xff\xff\xff\xff"));
            w.write_raw(ws, sbuf(
                "\xd5"));
            w.write_raw(ws, sbuf(
                "\x80\x81\xff\xff\xff\xff\xd5"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == "**");
        });

        // ping
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

        // ping
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
            doReadTest(w, ws, close_code::none);
        });

        // ping then message
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

        // ping then fragmented message
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
        });

        // already closed
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
            multi_buffer b;
            doFailTest(w, ws,
                boost::asio::error::operation_aborted);
        });

        // buffer overflow
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, buffer(s));
            try
            {
                multi_buffer b(3);
                w.read(ws, b);
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                if(se.code() != error::buffer_overflow)
                    throw;
            }
        });

        // bad utf8, big
        doTest(pmd, [&](ws_type& ws)
        {
            auto const s = std::string(2000, '*') +
                random_string();
            ws.text(true);
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // invalid fixed frame header
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x8f, 0x80, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::protocol_error);
        });

        // receive bad close
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x02, 0x03, 0xed));
            doFailTest(w, ws, error::failed);
        });

        // expected cont
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_some(ws, false, boost::asio::null_buffers{});
            w.write_raw(ws, cbuf(
                0x81, 0x80, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::protocol_error);
        });

        // message size above 2^64
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_some(ws, false, sbuf("*"));
            w.write_raw(ws, cbuf(
                0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::too_big);
        });

        // message size exceeds max
        doTest(pmd, [&](ws_type& ws)
        {
            ws.read_message_max(1);
            w.write(ws, sbuf("**"));
            doFailTest(w, ws, error::failed);
        });

        // unexpected cont
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x80, 0x80, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::protocol_error);
        });

        // bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            doFailTest(w, ws, error::failed);
        });

        // incomplete utf8
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s =
                "Hello, world!" "\xc0";
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // incomplete utf8, big
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s =
                random_string() +
                "Hello, world!" "\xc0";
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ios_};
                ws.next_layer().connect(es.stream());
                w.handshake(ws, "localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                try
                {
                    w.read(ws, b);
                    fail("", __FILE__, __LINE__);
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(se.code() == ev,
                        se.code().message());
                }
                ws.next_layer().close();
            };

            // payload length 1
            check(error::failed,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::failed,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::failed,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }

        //
        // permessage-deflate
        //

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 9;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;

        // message size limit
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = std::string(128, '*');
            w.write(ws, buffer(s));
            ws.read_message_max(32);
            doFailTest(w, ws, error::failed);
        });

        // invalid inflate block
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            ws.next_layer().append(
                "\xc2\x40" + s.substr(0, 64));
            flat_buffer b;
            try
            {
                w.read(ws, b);
            }
            catch(system_error const& se)
            {
                if(se.code() == test::error::fail_error)
                    throw;
                BEAST_EXPECTS(se.code().category() ==
                    zlib::detail::get_error_category(),
                        se.code().message());
            }
            catch(...)
            {
                throw;
            }
        });

        // no_context_takeover
        pmd.server_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });
        pmd.client_no_context_takeover = false;
    }

    template<class Wrap>
    void
    doTestRead(
        permessage_deflate const& pmd,
        Wrap const& w)
    {
        using boost::asio::buffer;

        // message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // empty message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "";
            ws.text(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // partial message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            w.write(ws, buffer(s));
            char buf[3];
            auto const bytes_written =
                w.read_some(ws, buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(
                string_view(buf, 3).substr(0, bytes_written) ==
                    s.substr(0, bytes_written));
        });

        // partial message, dynamic buffer
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            w.write(ws, buffer(s));
            multi_buffer b;
            auto bytes_written =
                w.read_some(ws, 3, b);
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(to_string(b.data()) ==
                s.substr(0, b.size()));
            w.read_some(ws, 256, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // big message
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // message, bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "\x03\xea\xf0\x28\x8c\xbc";
            ws.auto_fragment(false);
            ws.text(true);
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });
    }

    void
    testRead()
    {
        using boost::asio::buffer;

        doTestRead(SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(AsyncClient{yield});
        });

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;
        doTestRead(pmd, SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(pmd, AsyncClient{yield});
        });

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 9;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;
        doTestRead(pmd, SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(pmd, AsyncClient{yield});
        });

        // Read close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ios_};
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                ws.read(b, ec);
                BEAST_EXPECTS(ec == ev, ec.message());
                ws.next_layer().close();
            };

            // payload length 1
            check(error::failed,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::failed,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::failed,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }

        // suspend on write
        {
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            // insert a ping
            ws.next_layer().append(string_view(
                "\x89\x00", 2));
            std::size_t count = 0;
            multi_buffer b;
            std::string const s = "Hello, world";
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                });
            ws.async_write(buffer(s),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ios.run();
            BEAST_EXPECT(count == 2);
        }
    }

    void
    run() override
    {
        testRead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read);

} // websocket
} // beast
} // boost
