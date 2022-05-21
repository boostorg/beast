//
// Copyright (c) 2022 Seth Heeren (sgheeren at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/message_generator.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp> //for operator<<

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/buffers_generator.hpp> // for is_buffers_generator and [async_]write overloads
#include <boost/beast/_experimental/test/stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <iostream>
#include <iomanip>

namespace boost {
namespace beast {
namespace http {

namespace detail {

// Degenerate body generator to trigger dynamic generator buffer
// allocation
//
// Warning: magic numbers ahead
//
// Arbitrarily decided on 65 buffers of which two are "some" and "body", the
// other bufs are chosen by the "seed fragment"
struct fragmented_test_body
{
    using value_type = net::const_buffer;

    static std::uint64_t
    size(net::const_buffer fragment)
    {
        return 8 + 63 * fragment.size();
    }

    struct writer
    {
        using const_buffers_type = std::array<net::const_buffer, 65>;

        bool done_ = false;
        net::const_buffer seed_fragment_;

        template<typename Fields>
        writer(Fields const&, net::const_buffer fragment)
            : seed_fragment_(fragment)
        {
        }

        void
        init(beast::error_code&) const
        {
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(beast::error_code&)
        {
            const_buffers_type cb;
            std::fill(begin(cb), end(cb), seed_fragment_);
            cb.at(7) = { "some", 4 };
            cb.at(27) = { "body", 4 };
            return std::make_pair(cb, exchange(done_, true));
        }
    };
};
}

namespace detail {
template<>
struct is_body_sized<detail::fragmented_test_body>
    : std::true_type
{
};
} // namespace detail

class message_generator_test : public beast::unit_test::suite
{
    static_assert(
        is_buffers_generator<message_generator>::value,
        "buffers_generator not modeled");
    BOOST_STATIC_ASSERT(
        std::is_constructible<
            message_generator,
            message<true, string_body>&&>::value);
    BOOST_STATIC_ASSERT(
        std::is_constructible<
            message_generator,
            message<false, string_body>&&>::value);

    // only rvalue refs
    BOOST_STATIC_ASSERT(
        not std::is_constructible<
            message_generator,
            message<true, string_body>&>::value);
    BOOST_STATIC_ASSERT(
        not std::is_constructible<
            message_generator,
            message<true, string_body> const&>::value);

    static request<string_body> make_get() {
        return request<string_body>{
            verb::get, "/path/query?1", 11,
            "Serializable but ignored on GET"
        };
    }

    static http::response<detail::fragmented_test_body>
    make_fragmented_body_response(net::const_buffer seed_fragment)
    {
        http::response<detail::fragmented_test_body> msg(
            http::status::ok, 11, seed_fragment);
        msg.prepare_payload();

        BEAST_EXPECT(msg.has_content_length());
        BEAST_EXPECT(
            msg.at(http::field::content_length) ==
            std::to_string(8 + 63 * seed_fragment.size()));
        return msg;
    }

public:
    void
    testGenerate()
    {
        message_generator gen(make_get());
        error_code ec;

        std::string received;

        while(! gen.is_done())
        {
            message_generator::const_buffers_type b = gen.prepare(ec);
            BEAST_EXPECT(! ec);
            received += buffers_to_string(b);

            gen.consume(buffer_bytes(b));
        }

        BEAST_EXPECT(received ==
                     "GET /path/query?1 HTTP/1.1\r\n\r\n"
                     "Serializable but ignored on GET");
    }

    void
    testGenerateSlowConsumer()
    {
        message_generator gen(make_get());
        error_code ec;

        std::vector<std::string> received;

        while(! gen.is_done())
        {
            message_generator::const_buffers_type b = gen.prepare(ec);
            BEAST_EXPECT(! ec);
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

            message_generator gen(make_get());

            beast::async_write(out, std::move(gen),
                        [&](error_code ec, size_t total) {
                            BEAST_EXPECT(total == 61);
                            BEAST_EXPECT(!ec.failed());
                        });

            ioc.run();
        }

        BEAST_EXPECT(61 == in.nwrite_bytes());
        BEAST_EXPECT(in.str() ==
                     "GET /path/query?1 HTTP/1.1\r\n\r\n"
                     "Serializable but ignored on GET");
    }

    void
    testWrite()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        test::connect(out, in);

        {
            message_generator gen(make_get());

            error_code ec;
            std::size_t total = beast::write(out, gen, ec);

            BEAST_EXPECT(total == 61);
            BEAST_EXPECT(!ec.failed());

            BEAST_EXPECT(61 == in.nwrite_bytes());
            BEAST_EXPECT(in.str() ==
                    "GET /path/query?1 HTTP/1.1\r\n\r\n"
                    "Serializable but ignored on GET");
        }

        in.clear();

        {
            // rvalue accepted
            std::size_t total =
                beast::write(out, message_generator{ make_get() });
            BEAST_EXPECT(total == 61);
            BEAST_EXPECT(in.str() ==
                    "GET /path/query?1 HTTP/1.1\r\n\r\n"
                    "Serializable but ignored on GET");
        }
    }

    void
    testFragmentedBody()
    {
        net::io_context ioc;
        test::stream out(ioc), in(ioc);
        test::connect(out, in);

        {
            message_generator gen(make_fragmented_body_response(net::buffer("", 0)));

            error_code ec;
            std::size_t total = beast::write(out, gen, ec);

            BEAST_EXPECT(total == 46);
            BEAST_EXPECT(! ec.failed());

            BEAST_EXPECT(
                in.str() ==
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 8\r\n\r\nsomebody");
        }

        in.clear();

        {
            message_generator gen(make_fragmented_body_response(net::buffer("x", 1)));

            error_code ec;
            std::size_t total = beast::write(out, gen, ec);

            BEAST_EXPECT(total == 47 + 63);
            BEAST_EXPECT(! ec.failed());

            BEAST_EXPECT(
                in.str() ==
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 71\r\n\r\nxxxxxxxsomexxxxxxxxxxxxxxxxxxxbodyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        }
    }

    void
    testKeepAlive()
    {
        auto request = [](int version)
        {
            return http::request<http::string_body>(
                http::verb::post, "/", version);
        };
        BEAST_EXPECT(
            ! http::message_generator(request(10)).keep_alive());
        BEAST_EXPECT(
            http::message_generator(request(11)).keep_alive());
    }

    void
    run() override
    {
        testGenerate();
        testGenerateSlowConsumer();
        testAsyncWrite();
        testWrite();
        testFragmentedBody();
        testKeepAlive();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,message_generator);

} // http
} // beast
} // boost
