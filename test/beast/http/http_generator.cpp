//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/http_generator.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/buffers_generator.hpp> // for is_buffers_generator
#include <boost/beast/_experimental/test/stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <iostream>
#include <iomanip>

namespace boost {
namespace beast {
namespace http {

class http_generator_test : public beast::unit_test::suite
{
    static_assert(
        is_buffers_generator<http_generator>::value,
        "buffers_generator not modeled");
    BOOST_STATIC_ASSERT(
        std::is_constructible<
            http_generator,
            message<true, string_body>&&>::value);
    BOOST_STATIC_ASSERT(
        std::is_constructible<
            http_generator,
            message<false, string_body>&&>::value);

    // only rvalue refs
    BOOST_STATIC_ASSERT(
        not std::is_constructible<
            http_generator,
            message<true, string_body>&>::value);
    BOOST_STATIC_ASSERT(
        not std::is_constructible<
            http_generator,
            message<true, string_body> const&>::value);

    static request<string_body> make_get() {
        return request<string_body>{
            verb::get, "/path/query?1", 11,
            "Serializable but ignored on GET"
        };
    }

public:
    void
    testGenerate()
    {
        http_generator gen(make_get());
        error_code ec;

        std::vector<std::string> received;
        http_generator::const_buffers_type b;

        while (buffer_bytes(b = gen.prepare(ec))) {
            BEAST_EXPECT(!ec);
            received.push_back(buffers_to_string(b));

            gen.consume(buffer_bytes(b));
        }

        BEAST_EXPECT(received.size() == 1);
        BEAST_EXPECT(received.front() ==
                     "GET /path/query?1 HTTP/1.1\r\n\r\n"
                     "Serializable but ignored on GET");
    }

    void
    testGenerateSlowConsumer()
    {
        http_generator gen(make_get());
        error_code ec;

        std::vector<std::string> received;
        http_generator::const_buffers_type b;

        while (buffer_bytes(b = gen.prepare(ec))) {
            BEAST_EXPECT(!ec);
            received.push_back(buffers_to_string(b).substr(0, 3));

            gen.consume(3); // allowed > buffer_bytes(b)
        }

        BEAST_EXPECT(
            (received ==
             std::vector<std::string>{
                 "GET", " /p", "ath", "/qu", "ery",
                 "?1 ", "HTT", "P/1", ".1\r", "\n\r\n",
                 "Ser", "ial", "iza", "ble", " bu",
                 "t i", "gno", "red", " on", " GE",
                 "T",
             }));
    }

    void
    testAsyncWrite()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        {
            test::connect(out, in);

            http_generator gen(make_get());

            async_write(
                out, gen, [&](error_code ec, size_t total) {
                    BEAST_EXPECT(total == 61);
                    BEAST_EXPECT(!ec.failed());
                });

            ioc.run();
        }

        BEAST_EXPECT(1 == out.nwrite());
        BEAST_EXPECT(61 == in.nwrite_bytes());
        BEAST_EXPECT(in.str() ==
                     "GET /path/query?1 HTTP/1.1\r\n\r\n"
                     "Serializable but ignored on GET");
    }

    void
    run() override
    {
        testGenerate();
        testGenerateSlowConsumer();
        testAsyncWrite();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,http_generator);

} // http
} // beast
} // boost
