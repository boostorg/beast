//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/read_size.hpp>

#include <boost/beast/core/drain_buffer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/unit_test/suite.hpp>

#include <boost/asio/streambuf.hpp>

namespace boost {
namespace beast {

class read_size_test : public beast::unit_test::suite
{
public:
    template<class DynamicBuffer>
    void
    check()
    {
        DynamicBuffer buffer;
        read_size(buffer, 65536);
        pass();
    }

    void
    run() override
    {
        check<drain_buffer>();
        check<flat_buffer>();
        check<multi_buffer>();
        check<boost::asio::streambuf>();
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(read_size,core,beast);

} // beast
} // boost
