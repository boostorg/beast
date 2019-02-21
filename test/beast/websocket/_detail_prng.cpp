//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/detail/prng.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class prng_test
    : public beast::unit_test::suite
{
public:
#if 0
    char const* name() const noexcept //override
    {
        return "boost.beast.websocket.detail.prng";
    }
#endif

    template <class F>
    void
    testPrng(F const& f)
    {
        {
            auto v = f()();
            BEAST_EXPECT(
                v >= (prng::ref::min)() &&
                v <= (prng::ref::max)());
        }
        {
            auto v = f()();
            BEAST_EXPECT(
                v >= (prng::ref::min)() &&
                v <= (prng::ref::max)());
        }
    }

    void
    run() override
    {
        testPrng([]{ return make_prng(true); });
        testPrng([]{ return make_prng(false); });
        testPrng([]{ return make_prng_no_tls(true); });
        testPrng([]{ return make_prng_no_tls(false); });
    #ifndef BOOST_NO_CXX11_THREAD_LOCAL
        testPrng([]{ return make_prng_tls(true); });
        testPrng([]{ return make_prng_tls(false); });
    #endif
    }
};

//static prng_test t;
BEAST_DEFINE_TESTSUITE(beast,websocket,prng);

} // detail
} // websocket
} // beast
} // boost
