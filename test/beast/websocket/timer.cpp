
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/asio/ip/tcp.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

class timer_test
    : public websocket_test_suite
{
public:
    using tcp = boost::asio::ip::tcp;

    void
    run() override
    {
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,timer);

} // websocket
} // beast
} // boost
