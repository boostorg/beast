//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_generator.hpp>

#include "test_buffer.hpp"

#include <boost/asio/error.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/detail/static_ostream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

#include <iostream>

namespace boost {
namespace beast {

namespace test_detail {

struct test_buffers_generator {
    using underlying_buffer_sequence = std::array<net::const_buffer, 2>;
    using const_buffers_type = buffers_suffix<underlying_buffer_sequence>;
    size_t iterations_ = 5;
    bool verbose_ = false;

    test_buffers_generator(bool verbose = false) : verbose_(verbose) {}

    const_buffers_type cur_{};

    const_buffers_type prepare(error_code& ec) {
        if (verbose_)
            std::clog
                << "prepare, iterations_:" << iterations_
                << " '" << buffers_to_string(cur_) << "' ";
        if (!buffer_bytes(cur_)) {
            if (iterations_) {
                cur_ = const_buffers_type(
                    underlying_buffer_sequence{
                        net::buffer("abcde", iterations_),
                        net::buffer("12345", iterations_),
                    });
                iterations_ -= 1;
            } else {
                ec = net::error::eof;
            }
        }

        if (verbose_)
            std::clog << " -> '" << buffers_to_string(cur_)
                      << "'\n";
        return const_buffers_type{ cur_ };
    }

    void consume(size_t n) {
        cur_.consume(n);
    }
};

} // namespace test_detail

class buffers_generator_test : public unit_test::suite
{
public:
    using Generator = test_detail::test_buffers_generator;

    void
    testMinimalGenerator()
    {
        static_assert(is_buffers_generator<Generator>::value,
                      "buffers_generator not modeled");

        Generator gen;
        error_code ec;

        std::vector<std::string> actual;
        Generator::const_buffers_type b;

        while (buffer_bytes(b = gen.prepare(ec))) {
            BEAST_EXPECT(!ec);
            actual.push_back(buffers_to_string(b));

            gen.consume(3); // okay if > buffer_bytes
        }
        BEAST_EXPECT(ec == net::error::eof);
        BEAST_EXPECT(
            (actual ==
             std::vector<std::string>{
                 "abcde12345", "de12345", "2345", "5",
                 "abcd1234", "d1234", "34", "abc123", "123",
                 "ab12", "2", "a1" }));
    }

    void
    testWrite()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            test_detail::test_buffers_generator gen;

            beast::error_code ec;
            auto total = write(out, gen, ec);
            BEAST_EXPECT(total == 30);
            BEAST_EXPECT(ec == net::error::eof);
        }

        BEAST_EXPECT(5 == out.nwrite());
        BEAST_EXPECT(0 == out.nwrite_bytes());
        BEAST_EXPECT(0 == out.nread_bytes());
        BEAST_EXPECT(0 == out.nread());

        BEAST_EXPECT(30 == in.nwrite_bytes());
        BEAST_EXPECT("abcde12345abcd1234abc123ab12a1" == in.str());
    }

    void
    testWriteException()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            test_detail::test_buffers_generator gen;

            try {
                auto total = write(out, gen);
                BEAST_EXPECT(!"unreachable");
                BEAST_EXPECT(total==30);
            } catch (system_error const& se) {
                BEAST_EXPECT(se.code() == net::error::eof);
            }
        }

        BEAST_EXPECT(5 == out.nwrite());
        BEAST_EXPECT(30 == in.nwrite_bytes());
        BEAST_EXPECT("abcde12345abcd1234abc123ab12a1" == in.str());
    }

    void
    testAsyncWrite()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            test_detail::test_buffers_generator gen;

            async_write(
                out, gen, [&](error_code ec, size_t total) {
                    BEAST_EXPECT(total == 30);
                    BEAST_EXPECT(ec == net::error::eof);
                });

            ioc.run();
        }

        BEAST_EXPECT(5 == out.nwrite());
        BEAST_EXPECT(0 == out.nwrite_bytes());
        BEAST_EXPECT(0 == out.nread_bytes());
        BEAST_EXPECT(0 == out.nread());

        BEAST_EXPECT(30 == in.nwrite_bytes());
        BEAST_EXPECT("abcde12345abcd1234abc123ab12a1" == in.str());
    }

    void
    testWriteFail()
    {
        net::io_context ioc;
        test::fail_count fc { 3 };
        test::stream out(ioc, fc), in(ioc);
        {
            test::connect(out, in);

            test_detail::test_buffers_generator gen;

            try {
                /*auto total =*/ write(out, gen);
                BEAST_EXPECT(!"unreachable");
            } catch (system_error const& se) {
                BEAST_EXPECT(se.code() == test::error::test_failure);
            }
        }

        BEAST_EXPECT(3 == out.nwrite());
        BEAST_EXPECT(18 == in.nwrite_bytes()); // first two writes 10+8
        BEAST_EXPECT("abcde12345abcd1234" == in.str());
    }

    void
    testAsyncWriteFail()
    {
        net::io_context ioc;
        test::fail_count fc { 3 };
        test::stream out(ioc, fc), in(ioc);
        {
            test::connect(out, in);

            test_detail::test_buffers_generator gen;

            async_write(
                out, gen, [&](error_code ec, size_t total) {
                    BEAST_EXPECT(total == 18);
                    BEAST_EXPECT(ec == test::error::test_failure);
                });

            ioc.run();
        }

        BEAST_EXPECT(3 == out.nwrite());

        BEAST_EXPECT(18 == in.nwrite_bytes()); // first two writes 10+8
        BEAST_EXPECT("abcde12345abcd1234" == in.str());
    }

    void
    run() override
    {
        testMinimalGenerator();
        testWrite();
        testWriteException();
        testAsyncWrite();

        testWriteFail();
        testAsyncWriteFail();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_generator);

} // beast
} // boost
