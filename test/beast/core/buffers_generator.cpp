//
// Copyright (c) 2022 Seth Heeren (sgheeren at gmail dot com)
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
#include <numeric>

namespace boost {
namespace beast {

namespace detail {

struct test_buffers_generator
{
    using underlying_buffer_sequence = std::array<net::const_buffer, 2>;
    using const_buffers_type = buffers_suffix<underlying_buffer_sequence>;
    std::size_t iterations_ = 5;
    bool verbose_ = false;
    error_code emulate_error_;

    test_buffers_generator(
        error_code emulate_error = {}, bool verbose = false)
        : verbose_(verbose)
        , emulate_error_(emulate_error)
    {
    }

    const_buffers_type cur_{};

    bool
    is_done() const
    {
        return iterations_ == 0 && ! buffer_bytes(cur_);
    }

    const_buffers_type
    prepare(error_code& ec)
    {
        ec = {};
        BEAST_EXPECT(! is_done());
        if(verbose_)
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
            }
            if(emulate_error_ && iterations_ == 3)
            {
                ec = emulate_error_; // generate the specified error
            }
        }

        if (verbose_)
            std::clog << " -> '" << buffers_to_string(cur_)
                      << "'\n";
        return const_buffers_type{ cur_ };
    }

    void consume(std::size_t n) {
        cur_.consume(n);
    }
};

} // namespace detail

class buffers_generator_test : public unit_test::suite
{
public:
    void
    testMinimalGenerator(error_code emulate_error)
    {
        static_assert(
            is_buffers_generator<
                detail::test_buffers_generator>::value,
            "buffers_generator not modeled");

        detail::test_buffers_generator gen{emulate_error};
        error_code ec;

        std::vector<std::string> actual;

        while(! gen.is_done())
        {
            detail::test_buffers_generator::const_buffers_type b =
                gen.prepare(ec);

            if (ec) {
                BEAST_EXPECT(emulate_error == ec);
                // In test we ignore the error because we know that's okay.
                //
                // For general models of BuffersGenerator behaviour is
                // unspecified when using a generator after receiving an error.
            }

            actual.push_back(buffers_to_string(b));

            gen.consume(3); // okay if > buffer_bytes
        }
        BEAST_EXPECT(! ec.failed());

        if(! emulate_error)
        {
            BEAST_EXPECT(
                    (actual ==
                     std::vector<std::string>{
                     "abcde12345", "de12345", "2345", "5",
                     "abcd1234", "d1234", "34", "abc123", "123",
                     "ab12", "2", "a1" }));
        }
    }

    void
    testWrite(error_code emulate_error)
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        test::connect(out, in);

        {
            detail::test_buffers_generator gen{emulate_error};

            beast::error_code ec;
            auto total = write(out, gen, ec);

            BEAST_EXPECT(ec == emulate_error);

            if(! emulate_error)
            {
                BEAST_EXPECT(total == 30);

                BEAST_EXPECT(5 == out.nwrite());
                BEAST_EXPECT(30 == in.nwrite_bytes());
                BEAST_EXPECT(
                    "abcde12345abcd1234abc123ab12a1" == in.str());
            } else
            {
                BEAST_EXPECT(total == 10);

                BEAST_EXPECT(1 == out.nwrite());
                BEAST_EXPECT(10 == in.nwrite_bytes());
                BEAST_EXPECT("abcde12345" == in.str());
            }
        }

        in.clear();

        {
            error_code ec;
            auto total = write(out, detail::test_buffers_generator{emulate_error}, ec);

            BEAST_EXPECT(ec == emulate_error);

            if(! emulate_error)
            {
                BEAST_EXPECT(total == 30);
                BEAST_EXPECT("abcde12345abcd1234abc123ab12a1" == in.str());
            } else
            {
                BEAST_EXPECT(total == 10);
                BEAST_EXPECT("abcde12345" == in.str());
            }
        }
    }

    void
    testWriteException(error_code emulate_error)
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            detail::test_buffers_generator gen{emulate_error};

            try {
                auto total = write(out, gen);
                if (emulate_error)
                    BEAST_EXPECT(!"unreachable");
                BEAST_EXPECT(total == 30);
            } catch(system_error const& se)
            {
                BEAST_EXPECT(se.code() == emulate_error);
            }
        }

        if(! emulate_error)
        {
            BEAST_EXPECT(5 == out.nwrite());
            BEAST_EXPECT(30 == in.nwrite_bytes());
            BEAST_EXPECT(
                "abcde12345abcd1234abc123ab12a1" == in.str());
        }
        else
        {
            BEAST_EXPECT(1 == out.nwrite());
            BEAST_EXPECT(10 == in.nwrite_bytes());
            BEAST_EXPECT("abcde12345" == in.str());
        }
    }

    void
    testAsyncWrite(error_code emulate_error)
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            detail::test_buffers_generator gen{emulate_error};

            async_write(
                out,
                gen,
                [&](error_code ec, std::size_t total)
                {
                    BEAST_EXPECT(ec == emulate_error);
                    if(! emulate_error)
                    {
                        BEAST_EXPECT(total == 30);
                    } else
                    {
                        BEAST_EXPECT(total == 10);
                    }
                });

            ioc.run();
        }

        if(! emulate_error)
        {
            BEAST_EXPECT(5 == out.nwrite());
            BEAST_EXPECT(30 == in.nwrite_bytes());
            BEAST_EXPECT(
                "abcde12345abcd1234abc123ab12a1" == in.str());
        }
        else
        {
            BEAST_EXPECT(1 == out.nwrite());
            BEAST_EXPECT(10 == in.nwrite_bytes());
            BEAST_EXPECT("abcde12345" == in.str());
        }
    }

    void
    testWriteFail()
    {
        net::io_context ioc;
        test::fail_count fc { 3 };
        test::stream out(ioc, fc), in(ioc);
        {
            test::connect(out, in);

            detail::test_buffers_generator gen;

            try {
                /*auto total =*/ write(out, gen);
                BEAST_EXPECT(! "unreachable");
            } catch(system_error const& se)
            {
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

            detail::test_buffers_generator gen;

            async_write(
                out, gen, [&](error_code ec, std::size_t total) {
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
        for(error_code emulate_error :
            {error_code{}, error_code{error::timeout}})
        {
            testMinimalGenerator(emulate_error);
            testWrite(emulate_error);
            testWriteException(emulate_error);
            testAsyncWrite(emulate_error);
        }

        testWriteFail();
        testAsyncWriteFail();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_generator);

} // beast
} // boost
