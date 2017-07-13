//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/detail/varint.hpp>

#include <beast/unit_test/suite.hpp>

namespace beast {

class varint_test : public beast::unit_test::suite
{
public:
    void
    testVarint()
    {
        using beast::detail::varint_read;
        using beast::detail::varint_size;
        using beast::detail::varint_write;
        std::size_t n0 = 0;
        std::size_t n1 = 1;
        for(;;)
        {
            char buf[16];
            BOOST_ASSERT(sizeof(buf) >= varint_size(n0));
            auto it = &buf[0];
            varint_write(it, n0);
            it = &buf[0];
            auto n = varint_read(it);
            BEAST_EXPECT(n == n0);
            n = n0 + n1;
            if(n < n1)
                break;
            n0 = n1;
            n1 = n;
        }
    }

    void
    run()
    {
        testVarint();
    }
};

BEAST_DEFINE_TESTSUITE(varint,core,beast);

} // beast
