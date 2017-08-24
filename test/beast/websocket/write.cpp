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

class stream_write_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestWrite(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            w.write_some(ws, false,
                boost::asio::buffer(s.data(), chop));
            w.write_some(ws, true, boost::asio::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            std::string const s = "Hello";
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask (large)
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.write_buffer_size(16);
            std::string const s(32, '*');
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask, autofrag
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(true);
            std::string const s(16384, '*');
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // nomask
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(false);
                std::string const s = "Hello";
                w.write(ws, boost::asio::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // nomask, autofrag
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(true);
                std::string const s(16384, '*');
                w.write(ws, boost::asio::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        pmd.client_enable = true;
        pmd.server_enable = true;

        // deflate
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            // This call should produce no
            // output due to compression latency.
            w.write_some(ws, false,
                boost::asio::buffer(s.data(), chop));
            w.write_some(ws, true, boost::asio::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, no context takeover
        pmd.client_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });
    }

    void
    testWrite()
    {
        doTestWrite(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestWrite(AsyncClient{yield});
        });

        // already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.write(sbuf(""), ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_write(sbuf(""),
                [&](error_code ec)
                {
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ios.run();
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
            std::size_t count = 0;
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_write(sbuf("*"),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ws.async_close({}, [&](error_code){});
            ios.run();
            BEAST_EXPECT(count == 2);
        }

        // suspend on write, nomask, frag
        {
            echo_server es{log, kind::async_client};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept(ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(boost::asio::buffer(s),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }

        // suspend on write, mask, frag
        {
            echo_server es{log, kind::async};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(boost::asio::buffer(s),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }

        // suspend on write, deflate
        {
            echo_server es{log, kind::async};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            {
                permessage_deflate pmd;
                pmd.client_enable = true;
                ws.set_option(pmd);
            }
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            auto const& s = random_string();
            ws.binary(true);
            ws.async_write(boost::asio::buffer(s),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }
    }

    /*
        https://github.com/boostorg/beast/issues/300

        Write a message as two individual frames
    */
    void
    testIssue300()
    {
        for(int i = 0; i < 2; ++i )
        {
            echo_server es{log, i==1 ?
                kind::async : kind::sync};
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());

            error_code ec;
            ws.handshake("localhost", "/", ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                return;
            ws.write_some(false, sbuf("u"));
            ws.write_some(true, sbuf("v"));
            multi_buffer b;
            ws.read(b, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
    }

    void
    testWriteSuspend()
    {
        for(int i = 0; i < 2; ++i )
        {
            echo_server es{log, i==1 ?
                kind::async : kind::sync};
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");

            // Make remote send a text message with bad utf8.
            ws.binary(true);
            put(ws.next_layer().buffer(), cbuf(
                0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            multi_buffer b;
            std::size_t count = 0;
            // Read text message with bad utf8.
            // Causes a close to be sent, blocking writes.
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    // Read should fail with protocol error
                    ++count;
                    BEAST_EXPECTS(
                        ec == error::failed, ec.message());
                    // Reads after failure are aborted
                    ws.async_read(b,
                        [&](error_code ec, std::size_t)
                        {
                            ++count;
                            BEAST_EXPECTS(ec == boost::asio::
                                error::operation_aborted,
                                    ec.message());
                        });
                });
            // Run until the read_op writes a close frame.
            while(! ws.wr_block_)
                ios.run_one();
            // Write a text message, leaving
            // the write_op suspended as a pausation.
            ws.async_write(sbuf("Hello"),
                [&](error_code ec)
                {
                    ++count;
                    // Send is canceled because close received.
                    BEAST_EXPECTS(ec == boost::asio::
                        error::operation_aborted,
                            ec.message());
                    // Writes after close are aborted.
                    ws.async_write(sbuf("World"),
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(ec == boost::asio::
                                error::operation_aborted,
                                    ec.message());
                        });
                });
            // Run until all completions are delivered.
            while(! ios.stopped())
                ios.run_one();
            BEAST_EXPECT(count == 4);
        }
    }

    void
    testAsyncWriteFrame()
    {
        for(int i = 0; i < 2; ++i)
        {
            for(;;)
            {
                echo_server es{log, i==1 ?
                    kind::async : kind::sync};
                boost::asio::io_service ios;
                stream<test::stream> ws{ios};
                ws.next_layer().connect(es.stream());

                error_code ec;
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.async_write_some(false,
                    boost::asio::null_buffers{},
                    [&](error_code)
                    {
                        fail();
                    });
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                //
                // Destruction of the io_service will cause destruction
                // of the write_some_op without invoking the final handler.
                //
                break;
            }
        }
    }

    void
    run() override
    {
        testWrite();
        testWriteSuspend();
        testIssue300();
        testAsyncWriteFrame();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_write);

} // websocket
} // beast
} // boost
