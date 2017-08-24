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

class stream_handshake_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestHandshake(Wrap const& w)
    {
        class req_decorator
        {
            bool& b_;

        public:
            req_decorator(req_decorator const&) = default;

            explicit
            req_decorator(bool& b)
                : b_(b)
            {
            }

            void
            operator()(request_type&) const
            {
                b_ = true;
            }
        };

        // handshake
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                w.handshake(ws, "localhost", "/");
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            response_type res;
            try
            {
                w.handshake(ws, res, "localhost", "/");
                // VFALCO validate res?
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, decorator
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            try
            {
                w.handshake_ex(ws, "localhost", "/",
                    req_decorator{called});
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response, decorator
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            response_type res;
            try
            {
                w.handshake_ex(ws, res, "localhost", "/",
                    req_decorator{called});
                // VFALCO validate res?
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });
    }

    void
    testHandshake()
    {
        doTestHandshake(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestHandshake(AsyncClient{yield});
        });

        auto const check =
        [&](std::string const& s)
        {
            stream<test::stream> ws{ios_};
            auto tr = connect(ws.next_layer());
            ws.next_layer().append(s);
            tr.close();
            try
            {
                ws.handshake("localhost:80", "/");
                fail();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECT(se.code() == error::handshake_failed);
            }
        };
        // wrong HTTP version
        check(
            "HTTP/1.0 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // wrong status
        check(
            "HTTP/1.1 200 OK\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing upgrade token
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: HTTP/2\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing connection token
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing accept key
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // wrong accept key
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: *\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
    }

    void
    run() override
    {
        testHandshake();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_handshake);

} // websocket
} // beast
} // boost
