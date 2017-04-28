//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/read.hpp>

#include "test_parser.hpp"

#include <beast/http/fields.hpp>
#include <beast/http/dynamic_body.hpp>
#include <beast/http/header_parser.hpp>
#include <beast/http/string_body.hpp>
#include <beast/test/fail_stream.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/spawn.hpp>
#include <atomic>

namespace beast {
namespace http {

class read_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    template<bool isRequest>
    void
    failMatrix(char const* s, yield_context do_yield)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        static std::size_t constexpr limit = 100;
        std::size_t n;
        auto const len = strlen(s);
        for(n = 0; n < limit; ++n)
        {
            streambuf sb;
            sb.commit(buffer_copy(
                sb.prepare(len), buffer(s, len)));
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_istream> fs{fc, ios_, ""};
            test_parser<isRequest> p(fc);
            error_code ec;
            read(fs, sb, p, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            static std::size_t constexpr pre = 10;
            streambuf sb;
            sb.commit(buffer_copy(
                sb.prepare(pre), buffer(s, pre)));
            test::fail_counter fc(n);
            test::fail_stream<test::string_istream> fs{
                fc, ios_, std::string{s + pre, len - pre}};
            test_parser<isRequest> p(fc);
            error_code ec;
            read(fs, sb, p, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            streambuf sb;
            sb.commit(buffer_copy(
                sb.prepare(len), buffer(s, len)));
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_istream> fs{fc, ios_, ""};
            test_parser<isRequest> p(fc);
            error_code ec;
            async_read(fs, sb, p, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            static std::size_t constexpr pre = 10;
            streambuf sb;
            sb.commit(buffer_copy(
                sb.prepare(pre), buffer(s, pre)));
            test::fail_counter fc(n);
            test::fail_stream<test::string_istream> fs{
                fc, ios_, std::string{s + pre, len - pre}};
            test_parser<isRequest> p(fc);
            error_code ec;
            async_read(fs, sb, p, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
    }

    void testThrow()
    {
        try
        {
            streambuf sb;
            test::string_istream ss(ios_, "GET / X");
            message_parser<true, dynamic_body, fields> p;
            read(ss, sb, p);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    void testFailures(yield_context do_yield)
    {
        char const* req[] = {
            "GET / HTTP/1.0\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Empty:\r\n"
            "\r\n"
            ,
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Content-Length: 2\r\n"
            "\r\n"
            "**"
            ,
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "10\r\n"
            "****************\r\n"
            "0\r\n\r\n"
            ,
            nullptr
        };

        char const* res[] = {
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            ,
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            "***"
            ,
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 3\r\n"
            "\r\n"
            "***"
            ,
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "10\r\n"
            "****************\r\n"
            "0\r\n\r\n"
            ,
            nullptr
        };

        for(std::size_t i = 0; req[i]; ++i)
            failMatrix<true>(req[i], do_yield);

        for(std::size_t i = 0; res[i]; ++i)
            failMatrix<false>(res[i], do_yield);
    }

    void testRead(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;

        for(n = 0; n < limit; ++n)
        {
            test::fail_stream<test::string_istream> fs(n, ios_,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            );
            request<dynamic_body> m;
            try
            {
                streambuf sb;
                read(fs, sb, m);
                break;
            }
            catch(std::exception const&)
            {
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_stream<test::string_istream> fs(n, ios_,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            );
            request<dynamic_body> m;
            error_code ec;
            streambuf sb;
            read(fs, sb, m, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_stream<test::string_istream> fs(n, ios_,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            );
            request<dynamic_body> m;
            error_code ec;
            streambuf sb;
            async_read(fs, sb, m, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
    }

    void
    testEof(yield_context do_yield)
    {
        {
            streambuf sb;
            test::string_istream ss(ios_, "");
            message_parser<true, dynamic_body, fields> p;
            error_code ec;
            read(ss, sb, p, ec);
            BEAST_EXPECT(ec == boost::asio::error::eof);
        }
        {
            streambuf sb;
            test::string_istream ss(ios_, "");
            message_parser<true, dynamic_body, fields> p;
            error_code ec;
            async_read(ss, sb, p, do_yield[ec]);
            BEAST_EXPECT(ec == boost::asio::error::eof);
        }
    }

    // Ensure completion handlers are not leaked
    struct handler
    {
        static std::atomic<std::size_t>&
        count() { static std::atomic<std::size_t> n; return n; }
        handler() { ++count(); }
        ~handler() { --count(); }
        handler(handler const&) { ++count(); }
        void operator()(error_code const&) const {}
    };

    void
    testIoService()
    {
        {
            // Make sure handlers are not destroyed
            // after calling io_service::stop
            boost::asio::io_service ios;
            test::string_istream is{ios,
                "GET / HTTP/1.1\r\n\r\n"};
            BEAST_EXPECT(handler::count() == 0);
            streambuf sb;
            message<true, dynamic_body, fields> m;
            async_read(is, sb, m, handler{});
            BEAST_EXPECT(handler::count() > 0);
            ios.stop();
            BEAST_EXPECT(handler::count() > 0);
            ios.reset();
            BEAST_EXPECT(handler::count() > 0);
            ios.run_one();
            BEAST_EXPECT(handler::count() == 0);
        }
        {
            // Make sure uninvoked handlers are
            // destroyed when calling ~io_service
            {
                boost::asio::io_service ios;
                test::string_istream is{ios,
                    "GET / HTTP/1.1\r\n\r\n"};
                BEAST_EXPECT(handler::count() == 0);
                streambuf sb;
                message<true, dynamic_body, fields> m;
                async_read(is, sb, m, handler{});
                BEAST_EXPECT(handler::count() > 0);
            }
            BEAST_EXPECT(handler::count() == 0);
        }
    }

    void
    run() override
    {
        testThrow();

        yield_to(&read_test::testFailures, this);
        yield_to(&read_test::testRead, this);
        yield_to(&read_test::testEof, this);

        testIoService();
    }
};

BEAST_DEFINE_TESTSUITE(read,http,beast);

} // http
} // beast
