//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream_base.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {

class stream_base_test : public unit_test::suite
{
public:
    void
    testJavadoc()
    {
    }

    void
    run() override
    {
        testJavadoc();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_base);

} // websocket
} // beast
} // boost
