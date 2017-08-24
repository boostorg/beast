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

class stream_close_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestClose(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // close
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
        });

        // close with code
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, close_code::going_away);
        });

        // double close
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            w.close(ws, {});
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // drain a message after close
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x01\x2a");
            w.close(ws, {});
        });

        // drain a big message after close
        {
            std::string s;
            s = "\x81\x7e\x10\x01";
            s.append(4097, '*');
            doTest(pmd, [&](ws_type& ws)
            {
                ws.next_layer().append(s);
                w.close(ws, {});
            });
        }

        // drain a ping after close
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x89\x01*");
            w.close(ws, {});
        });

        // drain invalid message frame after close
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x81\x81\xff\xff\xff\xff*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // drain invalid close frame after close
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x88\x01*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // close with incomplete read message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x02**");
            static_buffer<1> b;
            w.read_some(ws, 1, b);
            w.close(ws, {});
        });
    }

    void
    testClose()
    {
        doTestClose(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestClose(AsyncClient{yield});
        });

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
            ws.async_close({},
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            BEAST_EXPECT(count == 2);
        }

        // suspend on read
        {
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            flat_buffer b;
            std::size_t count = 0;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == error::closed, ec.message());
                });
            BEAST_EXPECT(ws.rd_block_);
            ws.async_close({},
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            BEAST_EXPECT(ws.wr_close_);
            ios.run();
            BEAST_EXPECT(count == 2);
        }
    }

    void
    testCloseSuspend()
    {
        echo_server es{log, kind::async};
        boost::asio::io_service ios;
        stream<test::stream> ws{ios};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");

        // Cause close to be received
        es.async_close();

        multi_buffer b;
        std::size_t count = 0;
        ws.async_read(b,
            [&](error_code ec, std::size_t)
            {
                ++count;
                BEAST_EXPECTS(ec == error::closed,
                    ec.message());
            });
        while(! ws.wr_block_)
            ios.run_one();
        // try to close
        ws.async_close("payload",
            [&](error_code ec)
            {
                ++count;
                BEAST_EXPECTS(ec == boost::asio::
                    error::operation_aborted,
                        ec.message());
            });
        static std::size_t constexpr limit = 100;
        std::size_t n;
        for(n = 0; n < limit; ++n)
        {
            if(count >= 2)
                break;
            ios.run_one();
        }
        BEAST_EXPECT(n < limit);
        ios.run();
    }

    void
    run() override
    {
        testClose();
        testCloseSuspend();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_close);

} // websocket
} // beast
} // boost
