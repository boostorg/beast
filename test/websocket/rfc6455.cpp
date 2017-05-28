//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/websocket/rfc6455.hpp>

#include <beast/unit_test/suite.hpp>

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
        BEAST_EXPECT(! is_upgrade(req));
        req.version = 11;
        req.method("POST");
        req.target("/");
        BEAST_EXPECT(! is_upgrade(req));
        req.method("GET");
        req.fields.insert("Connection", "upgrade");
        BEAST_EXPECT(! is_upgrade(req));
        req.fields.insert("Upgrade", "websocket");
        BEAST_EXPECT(! is_upgrade(req));
        req.fields.insert("Sec-WebSocket-Version", "13");
        BEAST_EXPECT(is_upgrade(req));
    }

    void
    run() override
    {
        test_is_upgrade();
    }
};

BEAST_DEFINE_TESTSUITE(rfc6455,websocket,beast);

} // websocket
} // beast
