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

class stream_ping_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestPing(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // ping
        doTest(pmd, [&](ws_type& ws)
        {
            w.ping(ws, {});
        });

        // pong
        doTest(pmd, [&](ws_type& ws)
        {
            w.pong(ws, {});
        });
    }

    void
    testPing()
    {
        doTestPing(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestPing(AsyncClient{yield});
        });

        // ping, already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.ping({}, ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async_ping, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_ping({},
                [&](error_code ec)
                {
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ios.run();
        }

        // pong, already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.pong({}, ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async_pong, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_pong({},
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
            ws.async_write(sbuf("*"),
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
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ws.async_close({}, [&](error_code){});
            ios.run();
            BEAST_EXPECT(count == 2);
        }
    }

    void
    testPingSuspend()
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
        // Read a close frame.
        // Sends a close frame, blocking writes.
        ws.async_read(b,
            [&](error_code ec, std::size_t)
            {
                // Read should complete with error::closed
                ++count;
                BEAST_EXPECTS(ec == error::closed,
                    ec.message());
                // Pings after a close are aborted
                ws.async_ping("",
                    [&](error_code ec)
                    {
                        ++count;
                        BEAST_EXPECTS(ec == boost::asio::
                            error::operation_aborted,
                                ec.message());
                    });
            });
        if(! BEAST_EXPECT(run_until(ios, 100,
                [&]{ return ws.wr_close_; })))
            return;
        // Try to ping
        ws.async_ping("payload",
            [&](error_code ec)
            {
                // Pings after a close are aborted
                ++count;
                BEAST_EXPECTS(ec == boost::asio::
                    error::operation_aborted,
                        ec.message());
                // Subsequent calls to close are aborted
                ws.async_close({},
                    [&](error_code ec)
                    {
                        ++count;
                        BEAST_EXPECTS(ec == boost::asio::
                            error::operation_aborted,
                                ec.message());
                    });
            });
        static std::size_t constexpr limit = 100;
        std::size_t n;
        for(n = 0; n < limit; ++n)
        {
            if(count >= 4)
                break;
            ios.run_one();
        }
        BEAST_EXPECT(n < limit);
        ios.run();
    }

    void
    run() override
    {
        testPing();
        testPingSuspend();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_ping);

} // websocket
} // beast
} // boost
