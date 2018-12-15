//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_range.hpp>

#include "buffer_test.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {

class buffers_range_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<
            decltype(beast::buffers_range(
                std::declval<net::mutable_buffer>()))>::value);

    void
    testBufferSequence()
    {
        {
            string_view s = "Hello, world!";
            test_buffer_sequence(*this, buffers_range(
                net::const_buffer{s.data(), s.size()}));
        }
        {
            char buf[13];
            test_buffer_sequence(*this,
                buffers_range(net::mutable_buffer{
                    buf, sizeof(buf)}));
        }
    }

    void
    run() override
    {
        testBufferSequence();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_range);

} // beast
} // boost
