//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/message_parser.hpp>

#include "test_parser.hpp"

#include <beast/unit_test/suite.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/core/flat_streambuf.hpp>
#include <beast/core/streambuf.hpp>
#include <beast/http/header_parser.hpp>
#include <beast/http/read.hpp>
#include <beast/http/parse.hpp>
#include <beast/http/string_body.hpp>
#include <boost/system/system_error.hpp>

namespace beast {
namespace http {

class message_parser_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    template<bool isRequest, class Pred>
    void
    testMatrix(std::string const& s, Pred const& pred)
    {
        beast::test::string_istream ss{get_io_service(), s};
        error_code ec;
    #if 0
        streambuf dynabuf;
    #else
        flat_streambuf dynabuf;
        dynabuf.reserve(1024);
    #endif
        message<isRequest, string_body, fields> m;
        read(ss, dynabuf, m, ec);
        if(! BEAST_EXPECTS(! ec, ec.message()))
            return;
        pred(m);
    }

    void
    testRead()
    {
        testMatrix<false>(
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            "*******",
            [&](message<false, string_body, fields> const& m)
            {
                BEAST_EXPECTS(m.body == "*******",
                    "body='" + m.body + "'");
            }
        );
        testMatrix<false>(
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "*****\r\n"
            "2;a;b=1;c=\"2\"\r\n"
            "--\r\n"
            "0;d;e=3;f=\"4\"\r\n"
            "Expires: never\r\n"
            "MD5-Fingerprint: -\r\n"
            "\r\n",
            [&](message<false, string_body, fields> const& m)
            {
                BEAST_EXPECT(m.body == "*****--");
            }
        );
        testMatrix<false>(
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****",
            [&](message<false, string_body, fields> const& m)
            {
                BEAST_EXPECT(m.body == "*****");
            }
        );
        testMatrix<true>(
            "GET / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "\r\n",
            [&](message<true, string_body, fields> const& m)
            {
            }
        );
        testMatrix<true>(
            "GET / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "X: \t x \t \r\n"
            "\r\n",
            [&](message<true, string_body, fields> const& m)
            {
                BEAST_EXPECT(m.fields["X"] == "x");
            }
        );
    }

    void
    testParse()
    {
        using boost::asio::buffer;
        {
            error_code ec;
            message_parser<true, string_body, fields> p;
            std::string const s =
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*";
            p.write(buffer(s), ec);
            auto const& m = p.get();
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(m.method == "GET");
            BEAST_EXPECT(m.url == "/");
            BEAST_EXPECT(m.version == 11);
            BEAST_EXPECT(m.fields["User-Agent"] == "test");
            BEAST_EXPECT(m.body == "*");
        }
        {
            error_code ec;
            message_parser<false, string_body, fields> p;
            std::string const s =
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*";
            p.write(buffer(s), ec);
            auto const& m = p.get();
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(m.status == 200);
            BEAST_EXPECT(m.reason == "OK");
            BEAST_EXPECT(m.version == 11);
            BEAST_EXPECT(m.fields["Server"] == "test");
            BEAST_EXPECT(m.body == "*");
        }
        // skip body
        {
            error_code ec;
            message_parser<false, string_body, fields> p;
            std::string const s =
                "HTTP/1.1 200 Connection Established\r\n"
                "Proxy-Agent: Zscaler/5.1\r\n"
                "\r\n";
            p.set_option(skip_body{true});
            p.write(buffer(s), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }
    }

    void
    testExpect100Continue()
    {
        test::string_istream ss{ios_,
            "POST / HTTP/1.1\r\n"
            "Expect: 100-continue\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****"};
        streambuf sb;
        error_code ec;
        header_parser<true, fields> p0;
        parse(ss, sb, p0, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(p0.got_header());
        BEAST_EXPECT(! p0.is_done());
        message_parser<true,
            string_body, fields> p1{std::move(p0)};
        parse(ss, sb, p1, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(p1.get().body == "*****");
    }

    void
    run() override
    {
        testRead();
        testParse();
        testExpect100Continue();
    }
};

BEAST_DEFINE_TESTSUITE(message_parser,http,beast);

} // http
} // beast

