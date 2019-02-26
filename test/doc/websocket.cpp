//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4459) // declaration hides global declaration
#endif

//[code_websocket_1a
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
//]

namespace {

//[code_websocket_1b
namespace net = boost::asio;
using namespace boost::beast;
using namespace boost::beast::websocket;

net::io_context ioc;
net::ssl::context ctx(net::ssl::context::sslv23);

//]

void
websocket_snippets()
{
    {
    //[code_websocket_1f

        // This newly constructed WebSocket stream will use the specified
        // I/O context and have support for the permessage-deflate extension.

        stream<tcp_stream> ws(ioc);

    //]
    }

    {
    //[code_websocket_2f

        // The `tcp_stream` will be constructed with a new strand which
        // uses the specified I/O context.

        stream<tcp_stream> ws(make_strand(ioc));

    //]
    }

    {
    //[code_websocket_3f

        // The WebSocket stream will use SSL and a new strand
        stream<ssl_stream<tcp_stream>> wss(make_strand(ioc), ctx);

        //
    //]
    }
}

} // (anon)

namespace boost {
namespace beast {

struct websocket_snippets_test
    : public beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&websocket_snippets);
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_snippets);

} // beast
} // boost

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
