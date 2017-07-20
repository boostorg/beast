//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/rfc6455.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {

class rfc6455_test
    : public beast::unit_test::suite
{
public:
    void
    test_is_upgrade()
    {
        http::header<true> req;
        req.version = 10;
        BOOST_BEAST_EXPECT(! is_upgrade(req));
        req.version = 11;
        req.method(http::verb::post);
        req.target("/");
        BOOST_BEAST_EXPECT(! is_upgrade(req));
        req.method(http::verb::get);
        req.insert("Connection", "upgrade");
        BOOST_BEAST_EXPECT(! is_upgrade(req));
        req.insert("Upgrade", "websocket");
        BOOST_BEAST_EXPECT(! is_upgrade(req));
        req.insert("Sec-WebSocket-Version", "13");
        BOOST_BEAST_EXPECT(is_upgrade(req));
    }

    void
    run() override
    {
        test_is_upgrade();
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(rfc6455,websocket,beast);

} // websocket
} // beast
} // boost
