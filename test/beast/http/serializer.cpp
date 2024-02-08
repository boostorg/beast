//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/serializer.hpp>

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class serializer_test : public beast::unit_test::suite
{
public:
    struct const_body
    {
        struct value_type{};

        struct writer
        {
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(header<isRequest, Fields> const&, value_type const&);

            void
            init(error_code& ec);

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code&);
        };
    };

    struct mutable_body
    {
        struct value_type{};

        struct writer
        {
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(header<isRequest, Fields>&, value_type&);

            void
            init(error_code& ec);

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code&);
        };
    };

    BOOST_STATIC_ASSERT(std::is_const<  serializer<
        true, const_body>::value_type>::value);

    BOOST_STATIC_ASSERT(! std::is_const<serializer<
        true, mutable_body>::value_type>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<
        serializer<true, const_body>,
        message   <true, const_body>&>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<
        serializer<true, const_body>,
        message   <true, const_body> const&>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<
        serializer<true, mutable_body>,
        message   <true, mutable_body>&>::value);

    BOOST_STATIC_ASSERT(! std::is_constructible<
        serializer<true, mutable_body>,
        message   <true, mutable_body> const&>::value);

    struct lambda
    {
        std::string msg;
        std::size_t size;

        template<class ConstBufferSequence>
        void
        operator()(error_code&,
            ConstBufferSequence const& buffers)
        {
            msg.append(buffers_to_string(buffers));
            size = buffer_bytes(buffers);
        }
    };

    void
    testWriteLimit()
    {
        auto const limit = 30;
        lambda visit;
        error_code ec;
        response<string_body> res;
        res.body().append(1000, '*');
        serializer<false, string_body> sr{res};
        sr.limit(limit);
        for(;;)
        {
            sr.next(ec, visit);
            BEAST_EXPECT(visit.size <= limit);
            sr.consume(visit.size);
            if(sr.is_done())
                break;
        }
    }

    void
    test_move_constructor()
    {
        using serializer_t = serializer<false, string_body>;
        response<string_body> m;
        m.version(10);
        m.result(status::ok);
        m.set(field::server, "test");
        m.set(field::content_length, "5");
        m.body() = "******************************";
        std::unique_ptr<serializer_t> sr{new serializer_t{m}};
        sr->limit(1);

        lambda visit;
        error_code ec;
        // Setting the limit to 1 necessitates multiple steps for the serializer to complete
        for(;;)
        {
            // Each time we use a newly move-constructed serializer
            sr.reset(new serializer_t{*sr});

            sr->next(ec, visit);
            sr->consume(visit.size);
            if(sr->is_done())
                break;
        }

        BEAST_EXPECT(sr->limit() == 1);
        BEAST_EXPECT(visit.msg ==
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "******************************");
    }

    void
    testIssue2221()
    {
        response<string_body> res;
        serializer<false, string_body> sr1{res};

        lambda visit;
        error_code ec;
        sr1.next(ec, visit);

        auto sr2 = std::move(sr1);
    }

    void
    run() override
    {
        testWriteLimit();
        test_move_constructor();
        testIssue2221();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,serializer);

} // http
} // beast
} // boost
