//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/test/stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {

class test_stream_test
    : public unit_test::suite
{
public:
    void
    testTestStream()
    {
        net::io_context ioc;
        char buf[1] = {};
        net::mutable_buffer m0;
        net::mutable_buffer m1(buf, sizeof(buf));

        {
            {
                test::stream ts(ioc);
            }
            {
                test::stream ts(ioc);
                ts.close();
            }
            {
                test::stream t1(ioc);
                auto t2 = connect(t1);
            }
            {
                test::stream t1(ioc);
                auto t2 = connect(t1);
                t2.close();
            }
            {
#if 0
                test::stream ts(ioc);
                error_code ec;
                ts.read_some(net::mutable_buffer{}, ec);
                log << ec.message();
#endif
            }
            {
                error_code ec;
                {
                    test::stream ts(ioc);
                    ts.async_read_some(m1,
                        [&](error_code ec_, std::size_t)
                        {
                            ec = ec_;
                        });
                }
                ioc.run();
                ioc.restart();
                BEAST_EXPECTS(
                    //ec == net::error::eof,
                    ec == net::error::operation_aborted,
                    ec.message());
            }
            {
                error_code ec;
                test::stream ts(ioc);
                ts.async_read_some(m1,
                    [&](error_code ec_, std::size_t)
                    {
                        ec = ec_;
                    });
                ts.close(); 
                ioc.run();
                ioc.restart();
                BEAST_EXPECTS(
                    //ec == net::error::eof,
                    ec == net::error::operation_aborted,
                    ec.message());
            }
            {
                error_code ec;
                test::stream t1(ioc);
                auto t2 = connect(t1);
                t1.async_read_some(m1,
                    [&](error_code ec_, std::size_t)
                    {
                        ec = ec_;
                    });
                t2.close();
                ioc.run();
                ioc.restart();
                BEAST_EXPECTS(
                    ec == net::error::eof,
                    ec.message());
            }
            {
                error_code ec;
                test::stream t1(ioc);
                auto t2 = connect(t1);
                t1.async_read_some(m1,
                    [&](error_code ec_, std::size_t)
                    {
                        ec = ec_;
                    });
                t1.close();
                ioc.run();
                ioc.restart();
                BEAST_EXPECTS(
                    ec == net::error::operation_aborted,
                    ec.message());
            }
        }
    }

    void
    run() override
    {
        testTestStream();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,test,test_stream);

} // beast
} // boost
