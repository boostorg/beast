//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/write.hpp>

#include <beast/http/buffer_body.hpp>
#include <beast/http/fields.hpp>
#include <beast/http/message.hpp>
#include <beast/http/read.hpp>
#include <beast/http/string_body.hpp>
#include <beast/core/error.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/test/fail_stream.hpp>
#include <beast/test/pipe_stream.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/error.hpp>
#include <sstream>
#include <string>

namespace beast {
namespace http {

class write_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    struct unsized_body
    {
        using value_type = std::string;

        class reader
        {
            value_type const& body_;

        public:
            using is_deferred = std::false_type;

            using const_buffers_type =
                boost::asio::const_buffers_1;

            template<bool isRequest, class Allocator>
            explicit
            reader(message<isRequest, unsized_body, Allocator> const& msg)
                : body_(msg.body)
            {
            }

            void
            init(error_code& ec)
            {
                beast::detail::ignore_unused(ec);
            }
            
            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code&)
            {
                return {{const_buffers_type{
                    body_.data(), body_.size()}, false}};
            }
        };
    };

    template<
        bool isDeferred,
        bool isSplit,
        bool isFinalEmpty
    >
    struct test_body
    {
        struct value_type
        {
            std::string s;
            bool mutable read = false;
        };

        class reader
        {
            int step_ = 0;
            value_type const& body_;

        public:
            using is_deferred =
                std::integral_constant<bool, isDeferred>;

            using const_buffers_type =
                boost::asio::const_buffers_1;

            template<bool isRequest, class Fields>
            explicit
            reader(message<isRequest, test_body,
                    Fields> const& msg)
                : body_(msg.body)
            {
            }

            void
            init(error_code&)
            {
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code&)
            {
                body_.read = true;
                return get(
                    std::integral_constant<bool, isSplit>{},
                    std::integral_constant<bool, isFinalEmpty>{});
            }

        private:
            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::false_type,    // isSplit
                std::false_type)    // isFinalEmpty
            {
                using boost::asio::buffer;
                if(body_.s.empty())
                    return boost::none;
                return {{buffer(body_.s.data(), body_.s.size()), false}};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::false_type,    // isSplit
                std::true_type)     // isFinalEmpty
            {
                using boost::asio::buffer;
                if(body_.s.empty())
                    return boost::none;
                switch(step_)
                {
                case 0:
                    step_ = 1;
                    return {{buffer(
                        body_.s.data(), body_.s.size()), true}};
                default:
                    return boost::none;
                }
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::true_type,     // isSplit
                std::false_type)    // isFinalEmpty
            {
                using boost::asio::buffer;
                auto const n = (body_.s.size() + 1) / 2;
                switch(step_)
                {
                case 0:
                    if(n == 0)
                        return boost::none;
                    step_ = 1;
                    return {{buffer(body_.s.data(), n),
                        body_.s.size() > 1}};
                default:
                    return {{buffer(body_.s.data() + n,
                        body_.s.size() - n), false}};
                }
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::true_type,     // isSplit
                std::true_type)     // isFinalEmpty
            {
                using boost::asio::buffer;
                auto const n = (body_.s.size() + 1) / 2;
                switch(step_)
                {
                case 0:
                    if(n == 0)
                        return boost::none;
                    step_ = body_.s.size() > 1 ? 1 : 2;
                    return {{buffer(body_.s.data(), n), true}};
                case 1:
                    BOOST_ASSERT(body_.s.size() > 1);
                    step_ = 2;
                    return {{buffer(body_.s.data() + n,
                        body_.s.size() - n), true}};
                default:
                    return boost::none;
                }
            }
        };
    };

    struct fail_body
    {
        class reader;

        class value_type
        {
            friend class reader;

            std::string s_;
            test::fail_counter& fc_;

        public:
            explicit
            value_type(test::fail_counter& fc)
                : fc_(fc)
            {
            }

            value_type&
            operator=(std::string s)
            {
                s_ = std::move(s);
                return *this;
            }
        };

        class reader
        {
            std::size_t n_ = 0;
            value_type const& body_;

        public:
            using is_deferred = std::false_type;

            using const_buffers_type =
                boost::asio::const_buffers_1;

            template<bool isRequest, class Allocator>
            explicit
            reader(message<isRequest, fail_body, Allocator> const& msg)
                : body_(msg.body)
            {
            }

            void
            init(error_code& ec)
            {
                body_.fc_.fail(ec);
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                if(body_.fc_.fail(ec))
                    return boost::none;
                if(n_ >= body_.s_.size())
                    return boost::none;
                return {{const_buffers_type{
                    body_.s_.data() + n_++, 1}, true}};
            }
        };
    };

    template<bool isRequest>
    bool
    equal_body(string_view sv, string_view body)
    {
        test::string_istream si{
            get_io_service(), sv.to_string()};
        message<isRequest, string_body, fields> m;
        multi_buffer b;
        try
        {
            read(si, b, m);
            return m.body == body;
        }
        catch(std::exception const& e)
        {
            log << "equal_body: " << e.what() << std::endl;
            return false;
        }
    }

    template<bool isRequest, class Body, class Fields>
    std::string
    str(message<isRequest, Body, Fields> const& m)
    {
        test::string_ostream ss(ios_);
        write(ss, m);
        return ss.str;
    }

    void
    testAsyncWrite(yield_context do_yield)
    {
        {
            message<false, string_body, fields> m;
            m.version = 10;
            m.result(status::ok);
            m.reason("OK");
            m.fields.insert("Server", "test");
            m.fields.insert("Content-Length", "5");
            m.body = "*****";
            error_code ec;
            test::string_ostream ss{ios_};
            async_write(ss, m, do_yield[ec]);
            if(BEAST_EXPECTS(! ec, ec.message()))
                BEAST_EXPECT(ss.str ==
                    "HTTP/1.0 200 OK\r\n"
                    "Server: test\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****");
        }
        {
            message<false, string_body, fields> m;
            m.version = 11;
            m.result(status::ok);
            m.reason("OK");
            m.fields.insert("Server", "test");
            m.fields.insert("Transfer-Encoding", "chunked");
            m.body = "*****";
            error_code ec;
            test::string_ostream ss(ios_);
            async_write(ss, m, do_yield[ec]);
            if(BEAST_EXPECTS(! ec, ec.message()))
                BEAST_EXPECT(ss.str ==
                    "HTTP/1.1 200 OK\r\n"
                    "Server: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "5\r\n"
                    "*****\r\n"
                    "0\r\n\r\n");
        }
    }

    void
    testFailures(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;

        for(n = 0; n < limit; ++n)
        {
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_ostream> fs(fc, ios_);
            message<true, fail_body, fields> m{fc};
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.fields.insert("Content-Length", "5");
            m.body = "*****";
            try
            {
                write(fs, m);
                BEAST_EXPECT(fs.next_layer().str ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                pass();
                break;
            }
            catch(std::exception const&)
            {
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_ostream> fs(fc, ios_);
            message<true, fail_body, fields> m{fc};
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.fields.insert("Transfer-Encoding", "chunked");
            m.body = "*****";
            error_code ec;
            write(fs, m, ec);
            if(ec == error::end_of_stream)
            {
                BEAST_EXPECT(fs.next_layer().str ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "0\r\n\r\n"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_ostream> fs(fc, ios_);
            message<true, fail_body, fields> m{fc};
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.fields.insert("Transfer-Encoding", "chunked");
            m.body = "*****";
            error_code ec;
            async_write(fs, m, do_yield[ec]);
            if(ec == error::end_of_stream)
            {
                BEAST_EXPECT(fs.next_layer().str ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "0\r\n\r\n"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_ostream> fs(fc, ios_);
            message<true, fail_body, fields> m{fc};
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.fields.insert("Content-Length", "5");
            m.body = "*****";
            error_code ec;
            write(fs, m, ec);
            if(! ec)
            {
                BEAST_EXPECT(fs.next_layer().str ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_counter fc(n);
            test::fail_stream<
                test::string_ostream> fs(fc, ios_);
            message<true, fail_body, fields> m{fc};
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.fields.insert("Content-Length", "5");
            m.body = "*****";
            error_code ec;
            async_write(fs, m, do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(fs.next_layer().str ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);
    }

    void
    testOutput()
    {
        // auto content-length HTTP/1.0
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m);
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.0\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            );
        }
        // keep-alive HTTP/1.0
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m, connection::keep_alive);
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.0\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "*"
            );
        }
        // upgrade HTTP/1.0
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            try
            {
                prepare(m, connection::upgrade);
                fail();
            }
            catch(std::exception const&)
            {
                pass();
            }
        }
        // no content-length HTTP/1.0
        {
            message<true, unsized_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 10;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m);
            test::string_ostream ss(ios_);
            error_code ec;
            write(ss, m, ec);
            BEAST_EXPECT(ec == error::end_of_stream);
            BEAST_EXPECT(ss.str ==
                "GET / HTTP/1.0\r\n"
                "User-Agent: test\r\n"
                "\r\n"
                "*"
            );
        }
        // auto content-length HTTP/1.1
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 11;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m);
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            );
        }
        // close HTTP/1.1
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 11;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m, connection::close);
            test::string_ostream ss(ios_);
            error_code ec;
            write(ss, m, ec);
            BEAST_EXPECT(ec == error::end_of_stream);
            BEAST_EXPECT(ss.str ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "Connection: close\r\n"
                "\r\n"
                "*"
            );
        }
        // upgrade HTTP/1.1
        {
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 11;
            m.fields.insert("User-Agent", "test");
            prepare(m, connection::upgrade);
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Connection: upgrade\r\n"
                "\r\n"
            );
        }
        // no content-length HTTP/1.1
        {
            message<true, unsized_body, fields> m;
            m.method(verb::get);
            m.target("/");
            m.version = 11;
            m.fields.insert("User-Agent", "test");
            m.body = "*";
            prepare(m);
            test::string_ostream ss(ios_);
            error_code ec;
            write(ss, m, ec);
            BEAST_EXPECT(ss.str ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            );
        }
    }

    void test_std_ostream()
    {
        // Conversion to std::string via operator<<
        message<true, string_body, fields> m;
        m.method(verb::get);
        m.target("/");
        m.version = 11;
        m.fields.insert("User-Agent", "test");
        m.body = "*";
        BEAST_EXPECT(boost::lexical_cast<std::string>(m) ==
            "GET / HTTP/1.1\r\nUser-Agent: test\r\n\r\n*");
        BEAST_EXPECT(boost::lexical_cast<std::string>(m.base()) ==
            "GET / HTTP/1.1\r\nUser-Agent: test\r\n\r\n");
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
            test::string_ostream os{ios};
            BEAST_EXPECT(handler::count() == 0);
            message<true, string_body, fields> m;
            m.method(verb::get);
            m.version = 11;
            m.target("/");
            m.fields.insert("Content-Length", 5);
            m.body = "*****";
            async_write(os, m, handler{});
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
                test::string_ostream is{ios};
                BEAST_EXPECT(handler::count() == 0);
                message<true, string_body, fields> m;
                m.method(verb::get);
                m.version = 11;
                m.target("/");
                m.fields.insert("Content-Length", 5);
                m.body = "*****";
                async_write(is, m, handler{});
                BEAST_EXPECT(handler::count() > 0);
            }
            BEAST_EXPECT(handler::count() == 0);
        }
    }

    template<class Stream,
        bool isRequest, class Body, class Fields,
            class Decorator = no_chunk_decorator>
    void
    do_write(Stream& stream, message<
        isRequest, Body, Fields> const& m, error_code& ec,
            Decorator const& decorator = Decorator{})
    {
        auto sr = make_serializer(m, decorator);
        for(;;)
        {
            stream.nwrite = 0;
            write_some(stream, sr, ec);
            if(ec)
                return;
            BEAST_EXPECT(stream.nwrite <= 1);
            if(sr.is_done())
                break;
        }
    }

    template<class Stream,
        bool isRequest, class Body, class Fields,
            class Decorator = no_chunk_decorator>
    void
    do_async_write(Stream& stream,
        message<isRequest, Body, Fields> const& m,
            error_code& ec, yield_context yield,
                Decorator const& decorator = Decorator{})
    {
        auto sr = make_serializer(m);
        for(;;)
        {
            stream.nwrite = 0;
            async_write_some(stream, sr, yield[ec]);
            if(ec)
                return;
            BEAST_EXPECT(stream.nwrite <= 1);
            if(sr.is_done())
                break;
        }
    }

    struct test_decorator
    {
        std::string s;

        template<class ConstBufferSequence>
        string_view
        operator()(ConstBufferSequence const& buffers)
        {
            s = ";x=" + std::to_string(boost::asio::buffer_size(buffers));
            return s;
        }

        string_view
        operator()(boost::asio::null_buffers)
        {
            return "Result: OK\r\n";
        }
    };

    template<class Body>
    void
    testWriteStream(boost::asio::yield_context yield)
    {
        test::pipe p{ios_};
        p.client.write_size(3);

        message<false, Body, fields> m0;
        m0.version = 11;
        m0.result(status::ok);
        m0.reason("OK");
        m0.fields.insert("Server", "test");
        m0.body.s = "Hello, world!\n";

        {
            std::string const result =
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "\r\n"
                "Hello, world!\n";
            {
                auto m = m0;
                error_code ec;
                do_write(p.client, m, ec);
                BEAST_EXPECT(p.server.str() == result);
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_async_write(p.client, m, ec, yield);
                BEAST_EXPECT(p.server.str() == result);
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                serializer<false, Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    write_some(p.client, sr);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body.read);
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                serializer<false, Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    async_write_some(p.client, sr, yield);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body.read);
                p.server.clear();
            }
        }
        {
            m0.fields.insert("Transfer-Encoding", "chunked");
            {
                auto m = m0;
                error_code ec;
                do_write(p.client, m, ec);
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_write(p.client, m, ec, test_decorator{});
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_async_write(p.client, m, ec, yield);
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_async_write(p.client, m, ec, yield, test_decorator{});
                BEAST_EXPECT(equal_body<false>(
                    p.server.str(), m.body.s));
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                test::string_ostream so{get_io_service(), 3};
                serializer<false, Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    write_some(p.client, sr);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body.read);
                p.server.clear();
            }
            {
                auto m = m0;
                error_code ec;
                serializer<false, Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    async_write_some(p.client, sr, yield);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body.read);
                p.server.clear();
            }
        }
    }

    void run() override
    {
        yield_to([&](yield_context yield){
            testAsyncWrite(yield); });
        yield_to([&](yield_context yield){
            testFailures(yield); });
        testOutput();
        test_std_ostream();
        testIoService();
        yield_to(
            [&](yield_context yield)
            {
                testWriteStream<test_body<false, false, false>>(yield);
                testWriteStream<test_body<false, false,  true>>(yield);
                testWriteStream<test_body<false,  true, false>>(yield);
                testWriteStream<test_body<false,  true,  true>>(yield);
                testWriteStream<test_body< true, false, false>>(yield);
                testWriteStream<test_body< true, false,  true>>(yield);
                testWriteStream<test_body< true,  true, false>>(yield);
                testWriteStream<test_body< true,  true,  true>>(yield);
            });
    }
};

BEAST_DEFINE_TESTSUITE(write,http,beast);

} // http
} // beast
