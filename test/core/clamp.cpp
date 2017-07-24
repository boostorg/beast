//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/clamp.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <climits>

namespace boost {
namespace beast {
namespace detail {

class clamp_test : public beast::unit_test::suite
{
public:
    void testClamp()
    {
        BOOST_BEAST_EXPECT(clamp(
            (std::numeric_limits<std::uint64_t>::max)()) ==
                (std::numeric_limits<std::size_t>::max)());
    }

    void run() override
    {
        testClamp();
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(clamp,core,beast);

} // detail
} // beast
} // boost
