//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/stranded_stream.hpp>

#include "stream_tests.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost {
namespace beast {

class stranded_stream_test
    : public beast::unit_test::suite
{
public:
    using tcp = net::ip::tcp;

    void
    testStream()
    {
        net::io_context ioc;
        {
            using ex_t = net::io_context::executor_type;
            auto ex = ioc.get_executor();
            stranded_stream<tcp, ex_t> s1(ioc);
            stranded_stream<tcp, ex_t> s2(ex);
            stranded_stream<tcp, ex_t> s3(ioc, tcp::v4());
            stranded_stream<tcp, ex_t> s4(std::move(s1));
            s2.next_layer() = tcp::socket(ioc);
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);
            BEAST_EXPECT(s4.get_executor() == ex);
        }
        {
            using ex_t = net::io_context::strand;
            auto ex = ex_t{ioc};
            stranded_stream<tcp, ex_t> s1(ex);
            stranded_stream<tcp, ex_t> s2(ex, tcp::v4());
            stranded_stream<tcp, ex_t> s3(std::move(s1));
            s2.next_layer() = tcp::socket(ioc);
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);
        }
        {
            using ex_t = net::io_context::executor_type;
            test_sync_stream<stranded_stream<tcp, ex_t>>();
            test_async_stream<stranded_stream<tcp, ex_t>>();
        }
    }

    void
    testJavadocs()
    {
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testStream();
        testJavadocs();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,stranded_stream);

} // beast
} // boost
