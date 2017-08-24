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

class stream_read_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestRead(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // Read close frames
        {
            // VFALCO What about asynchronous??

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

        pmd.client_enable = true;
        pmd.server_enable = true;

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
    }

    void
    testRead()
    {
        doTestRead(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestRead(AsyncClient{yield});
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
    }

    void
    run() override
    {
        testRead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_read);

} // websocket
} // beast
} // boost
