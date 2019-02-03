//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/dynamic_buffer_ref.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/read_until.hpp>

namespace boost {
namespace beast {

namespace {

template <class SyncReadStream>
std::size_t read_line (SyncReadStream& stream, flat_buffer& buffer)
{
    return net::read_until(stream, dynamic_buffer_ref(buffer), "\r\n");
}

} // (anon)

class dynamic_buffer_ref_test : public beast::unit_test::suite
{
public:
    void
    testJavadocs()
    {
        BEAST_EXPECT(static_cast<
            std::size_t(*)(test::stream&, flat_buffer&)>(
                &read_line<test::stream>));
    }

    void run() override
    {
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,dynamic_buffer_ref);

} // beast
} // boost
