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

#if 0
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
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
