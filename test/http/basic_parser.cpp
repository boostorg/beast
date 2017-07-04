//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/basic_parser.hpp>

#include "test_parser.hpp"

#include <beast/core/buffer_cat.hpp>
#include <beast/core/buffer_prefix.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/core/ostream.hpp>
#include <beast/http/parser.hpp>
#include <beast/http/string_body.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class basic_parser_test : public beast::unit_test::suite
{
public:
    enum parse_flag
    {
        chunked               =   1,
        connection_keep_alive =   2,
        connection_close      =   4,
        connection_upgrade    =   8,
        upgrade               =  16,
    };

    class expect_version
    {
        suite& s_;
        int version_;

    public:
        expect_version(suite& s, int version)
            : s_(s)
            , version_(version)
        {
        }

        template<class Parser>
        void
        operator()(Parser const& p) const
        {
            s_.BEAST_EXPECT(p.version == version_);
        }
    };

    class expect_status
    {
        suite& s_;
        int status_;

    public:
        expect_status(suite& s, int status)
            : s_(s)
            , status_(status)
        {
        }

        template<class Parser>
        void
        operator()(Parser const& p) const
        {
            s_.BEAST_EXPECT(p.status == status_);
        }
    };

    class expect_flags
    {
        suite& s_;
        unsigned flags_;

    public:
        expect_flags(suite& s, unsigned flags)
            : s_(s)
            , flags_(flags)
        {
        }

        template<class Parser>
        void
        operator()(Parser const& p) const
        {
            if(flags_ & parse_flag::chunked)
                s_.BEAST_EXPECT(p.is_chunked());
            if(flags_ & parse_flag::connection_keep_alive)
                s_.BEAST_EXPECT(p.is_keep_alive());
            if(flags_ & parse_flag::connection_close)
                s_.BEAST_EXPECT(! p.is_keep_alive());
            if(flags_ & parse_flag::upgrade)
                s_.BEAST_EXPECT(! p.is_upgrade());
        }
    };

    class expect_keepalive
    {
        suite& s_;
        bool v_;

    public:
        expect_keepalive(suite& s, bool v)
            : s_(s)
            , v_(v)
        {
        }

        template<class Parser>
        void
        operator()(Parser const& p) const
        {
            s_.BEAST_EXPECT(p.is_keep_alive() == v_);
        }
    };

    class expect_body
    {
        suite& s_;
        std::string const& body_;

    public:
        expect_body(expect_body&&) = default;

        expect_body(suite& s, std::string const& v)
            : s_(s)
            , body_(v)
        {
        }

        template<class Parser>
        void
        operator()(Parser const& p) const
        {
            s_.BEAST_EXPECT(p.body == body_);
        }
    };

    //--------------------------------------------------------------------------

    template<class Parser, class ConstBufferSequence, class Test>
    typename std::enable_if<
        is_const_buffer_sequence<ConstBufferSequence>::value>::type
    parsegrind(ConstBufferSequence const& buffers,
        Test const& test, bool skip = false)
    {
        auto const size = boost::asio::buffer_size(buffers);
        for(std::size_t i = 1; i < size - 1; ++i)
        {
            Parser p;
            p.eager(true);
            p.skip(skip);
            error_code ec;
            consuming_buffers<ConstBufferSequence> cb{buffers};
            auto n = p.put(buffer_prefix(i, cb), ec);
            if(! BEAST_EXPECTS(! ec ||
                    ec == error::need_more, ec.message()))
                continue;
            if(! BEAST_EXPECT(! p.is_done()))
                continue;
            cb.consume(n);
            n = p.put(cb, ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                continue;
            if(! BEAST_EXPECT(n == boost::asio::buffer_size(cb)))
                continue;
            if(p.need_eof())
            {
                p.put_eof(ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    continue;
            }
            if(! BEAST_EXPECT(p.is_done()))
                continue;
            test(p);
        }
        for(std::size_t i = 1; i < size - 1; ++i)
        {
            Parser p;
            p.eager(true);
            error_code ec;
            consuming_buffers<ConstBufferSequence> cb{buffers};
            cb.consume(i);
            auto n = p.put(buffer_cat(
                buffer_prefix(i, buffers), cb), ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                continue;
            if(! BEAST_EXPECT(n == size))
                continue;
            if(p.need_eof())
            {
                p.put_eof(ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    continue;
            }
            test(p);
        }
    }

    template<class Parser, class Test>
    void
    parsegrind(string_view msg, Test const& test, bool skip = false)
    {
        parsegrind<Parser>(boost::asio::const_buffers_1{
            msg.data(), msg.size()}, test, skip);
    }

    template<class Parser, class ConstBufferSequence>
    typename std::enable_if<
        is_const_buffer_sequence<ConstBufferSequence>::value>::type
    parsegrind(ConstBufferSequence const& buffers)
    {
        parsegrind<Parser>(buffers, [](Parser const&){});
    }

    template<class Parser>
    void
    parsegrind(string_view msg)
    {
        parsegrind<Parser>(msg, [](Parser const&){});
    }

    template<class Parser>
    void
    failgrind(string_view msg, error_code const& result)
    {
        for(std::size_t i = 1; i < msg.size() - 1; ++i)
        {
            Parser p;
            p.eager(true);
            error_code ec;
            consuming_buffers<boost::asio::const_buffers_1> cb{
                boost::in_place_init, msg.data(), msg.size()};
            auto n = p.put(buffer_prefix(i, cb), ec);
            if(ec == result)
            {
                pass();
                continue;
            }
            if(! BEAST_EXPECTS(
                ec == error::need_more, ec.message()))
                continue;
            if(! BEAST_EXPECT(! p.is_done()))
                continue;
            cb.consume(n);
            n = p.put(cb, ec);
            if(! ec)
                p.put_eof(ec);
            BEAST_EXPECTS(ec == result, ec.message());
        }
        for(std::size_t i = 1; i < msg.size() - 1; ++i)
        {
            Parser p;
            p.eager(true);
            error_code ec;
            p.put(buffer_cat(
                boost::asio::const_buffers_1{msg.data(), i},
                boost::asio::const_buffers_1{
                    msg.data() + i, msg.size() - i}), ec);
            if(! ec)
                p.put_eof(ec);
            BEAST_EXPECTS(ec == result, ec.message());
        }
    }

    //--------------------------------------------------------------------------

    void
    testFlatten()
    {
        parsegrind<test_parser<true>>(
            "GET / HTTP/1.1\r\n"
            "\r\n"
            );
        parsegrind<test_parser<true>>(
            "POST / HTTP/1.1\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****"
            );
        parsegrind<test_parser<false>>(
            "HTTP/1.1 403 Not Found\r\n"
            "\r\n"
            );
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****"
            );
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5;x\r\n*****\r\n"
            "0\r\nMD5: 0xff30\r\n"
            "\r\n"
            );
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "\r\n"
            "*****"
            );
    }

    void
    testObsFold()
    {
        auto const check =
            [&](std::string const& s, string_view value)
            {
                std::string m =
                    "GET / HTTP/1.1\r\n"
                    "f: " + s + "\r\n"
                    "\r\n";
                parsegrind<parser<true, string_body>>(m,
                    [&](parser<true, string_body> const& p)
                    {
                        BEAST_EXPECT(p.get()["f"] == value);
                    });
            };
        check("x",                      "x");
        check(" x",                     "x");
        check("\tx",                    "x");
        check(" \tx",                   "x");
        check("\t x",                   "x");
        check("x ",                     "x");
        check(" x\t",                   "x");
        check("\tx \t",                 "x");
        check(" \tx\t ",                "x");
        check("\t x  \t  ",             "x");
        check("\r\n x",                 "x");
        check(" \r\n x",                "x");
        check(" \r\n\tx",               "x");
        check(" \r\n\t x",              "x");
        check(" \r\n \tx",              "x");
        check("  \r\n \r\n \r\n x \t",  "x");
        check("xy",                     "xy");
        check("\r\n x",                 "x");
        check("\r\n  x",                "x");
        check("\r\n   xy",              "xy");
        check("\r\n \r\n \r\n x",       "x");
        check("\r\n \r\n  \r\n xy",     "xy");
        check("x\r\n y",                "x y");
        check("x\r\n y\r\n z ",         "x y z");
    }

    // Check that all callbacks are invoked
    void
    testCallbacks()
    {
        parsegrind<test_parser<true>>(
            "GET / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "Content-Length: 1\r\n"
            "\r\n"
            "*",
            [&](test_parser<true> const& p)
            {
                BEAST_EXPECT(p.got_on_begin     == 1);
                BEAST_EXPECT(p.got_on_field     == 2);
                BEAST_EXPECT(p.got_on_header    == 1);
                BEAST_EXPECT(p.got_on_body      == 1);
                BEAST_EXPECT(p.got_on_chunk     == 0);
                BEAST_EXPECT(p.got_on_complete  == 1);
            });
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 1\r\n"
            "\r\n"
            "*",
            [&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.got_on_begin     == 1);
                BEAST_EXPECT(p.got_on_field     == 2);
                BEAST_EXPECT(p.got_on_header    == 1);
                BEAST_EXPECT(p.got_on_body      == 1);
                BEAST_EXPECT(p.got_on_chunk     == 0);
                BEAST_EXPECT(p.got_on_complete  == 1);
            });
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "1\r\n*\r\n"
            "0\r\n\r\n",
            [&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.got_on_begin     == 1);
                BEAST_EXPECT(p.got_on_field     == 2);
                BEAST_EXPECT(p.got_on_header    == 1);
                BEAST_EXPECT(p.got_on_body      == 1);
                BEAST_EXPECT(p.got_on_chunk     == 1);
                BEAST_EXPECT(p.got_on_complete  == 1);
            });
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "1;x\r\n*\r\n"
            "0\r\n\r\n",
            [&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.got_on_begin     == 1);
                BEAST_EXPECT(p.got_on_field     == 2);
                BEAST_EXPECT(p.got_on_header    == 1);
                BEAST_EXPECT(p.got_on_body      == 1);
                BEAST_EXPECT(p.got_on_chunk     == 1);
                BEAST_EXPECT(p.got_on_complete  == 1);
            });
    }

    void
    testRequestLine()
    {
        using P = test_parser<true>;

        parsegrind<P>("GET /x HTTP/1.0\r\n\r\n");
        parsegrind<P>("!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz / HTTP/1.0\r\n\r\n");
        parsegrind<P>("GET / HTTP/1.0\r\n\r\n",         expect_version{*this, 10});
        parsegrind<P>("G / HTTP/1.1\r\n\r\n",           expect_version{*this, 11});
        // VFALCO TODO various forms of good request-target (uri)

        failgrind<P>("\tGET / HTTP/1.0\r\n"    "\r\n",  error::bad_method);
        failgrind<P>("GET\x01 / HTTP/1.0\r\n"  "\r\n",  error::bad_method);
        failgrind<P>("GET  / HTTP/1.0\r\n"     "\r\n",  error::bad_target);
        failgrind<P>("GET \x01 HTTP/1.0\r\n"   "\r\n",  error::bad_target);
        failgrind<P>("GET /\x01 HTTP/1.0\r\n"  "\r\n",  error::bad_target);
        // VFALCO TODO various forms of bad request-target (uri)
        failgrind<P>("GET /  HTTP/1.0\r\n"     "\r\n",  error::bad_version);
        failgrind<P>("GET / _TTP/1.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / H_TP/1.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HT_P/1.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTT_/1.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP_1.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/01.2\r\n"     "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/3.45\r\n"     "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/67.89\r\n"    "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/x.0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1.x\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1.0 \r\n"     "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1_0\r\n"      "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1.0\n\r\n"    "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1.0\n\r\r\n"  "\r\n",  error::bad_version);
        failgrind<P>("GET / HTTP/1.0\r\r\n"    "\r\n",  error::bad_version);
    }

    void
    testStatusLine()
    {
        using P = test_parser<false>;

        parsegrind<P>("HTTP/1.0 000 OK\r\n"   "\r\n",   expect_status{*this, 0});
        parsegrind<P>("HTTP/1.1 012 OK\r\n"   "\r\n",   expect_status{*this, 12});
        parsegrind<P>("HTTP/1.0 345 OK\r\n"   "\r\n",   expect_status{*this, 345});
        parsegrind<P>("HTTP/1.0 678 OK\r\n"   "\r\n",   expect_status{*this, 678});
        parsegrind<P>("HTTP/1.0 999 OK\r\n"   "\r\n",   expect_status{*this, 999});
        parsegrind<P>("HTTP/1.0 200 \tX\r\n"  "\r\n",   expect_version{*this, 10});
        parsegrind<P>("HTTP/1.1 200  X\r\n"   "\r\n",   expect_version{*this, 11});
        parsegrind<P>("HTTP/1.0 200 \r\n"     "\r\n");
        parsegrind<P>("HTTP/1.1 200 X \r\n"   "\r\n");
        parsegrind<P>("HTTP/1.1 200 X\t\r\n"  "\r\n");
        parsegrind<P>("HTTP/1.1 200 \x80\x81...\xfe\xff\r\n\r\n");
        parsegrind<P>("HTTP/1.1 200 !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\r\n\r\n");

        failgrind<P>("\rHTTP/1.0 200 OK\r\n"  "\r\n",   error::bad_version);
        failgrind<P>("\nHTTP/1.0 200 OK\r\n"  "\r\n",   error::bad_version);
        failgrind<P>(" HTTP/1.0 200 OK\r\n"   "\r\n",   error::bad_version);
        failgrind<P>("_TTP/1.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("H_TP/1.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HT_P/1.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTT_/1.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTTP_1.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTTP/01.2 200 OK\r\n"   "\r\n",   error::bad_version);
        failgrind<P>("HTTP/3.45 200 OK\r\n"   "\r\n",   error::bad_version);
        failgrind<P>("HTTP/67.89 200 OK\r\n"  "\r\n",   error::bad_version);
        failgrind<P>("HTTP/x.0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTTP/1.x 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTTP/1_0 200 OK\r\n"    "\r\n",   error::bad_version);
        failgrind<P>("HTTP/1.0  200 OK\r\n"   "\r\n",   error::bad_status);
        failgrind<P>("HTTP/1.0 0 OK\r\n"      "\r\n",   error::bad_status);
        failgrind<P>("HTTP/1.0 12 OK\r\n"     "\r\n",   error::bad_status);
        failgrind<P>("HTTP/1.0 3456 OK\r\n"   "\r\n",   error::bad_status);
        failgrind<P>("HTTP/1.0 200\r\n"       "\r\n",   error::bad_status);
        failgrind<P>("HTTP/1.0 200 \n\r\n"    "\r\n",   error::bad_reason);
        failgrind<P>("HTTP/1.0 200 \x01\r\n"  "\r\n",   error::bad_reason);
        failgrind<P>("HTTP/1.0 200 \x7f\r\n"  "\r\n",   error::bad_reason);
        failgrind<P>("HTTP/1.0 200 OK\n\r\n"  "\r\n",   error::bad_reason);
        failgrind<P>("HTTP/1.0 200 OK\r\r\n"  "\r\n",   error::bad_line_ending);
    }

    void
    testFields()
    {
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };

        using P = test_parser<true>;

        parsegrind<P>(m("f:\r\n"));
        parsegrind<P>(m("f: \r\n"));
        parsegrind<P>(m("f:\t\r\n"));
        parsegrind<P>(m("f: \t\r\n"));
        parsegrind<P>(m("f: v\r\n"));
        parsegrind<P>(m("f:\tv\r\n"));
        parsegrind<P>(m("f:\tv \r\n"));
        parsegrind<P>(m("f:\tv\t\r\n"));
        parsegrind<P>(m("f:\tv\t \r\n"));
        parsegrind<P>(m("f:\r\n \r\n"));
        parsegrind<P>(m("f:v\r\n"));
        parsegrind<P>(m("f: v\r\n u\r\n"));
        parsegrind<P>(m("!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz: v\r\n"));
        parsegrind<P>(m("f: !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81...\xfe\xff\r\n"));

        failgrind<P>(m(" f: v\r\n"),        error::bad_field);
        failgrind<P>(m("\tf: v\r\n"),       error::bad_field);
        failgrind<P>(m("f : v\r\n"),        error::bad_field);
        failgrind<P>(m("f\t: v\r\n"),       error::bad_field);
        failgrind<P>(m("f: \n\r\n"),        error::bad_value);
        failgrind<P>(m("f: v\r \r\n"),      error::bad_line_ending);
        failgrind<P>(m("f: \r v\r\n"),      error::bad_line_ending);
        failgrind<P>(
            "GET / HTTP/1.1\r\n"
            "\r \n\r\n"
            "\r\n",                         error::bad_line_ending);
    }

    void
    testConnectionField()
    {
        auto const m = [](std::string const& s)
            { return "GET / HTTP/1.1\r\n" + s + "\r\n"; };
        auto const cn = [](std::string const& s)
            { return "GET / HTTP/1.1\r\nConnection: " + s + "\r\n"; };
    #if 0
        auto const keepalive = [&](bool v)
            { //return keepalive_f{*this, v}; return true; };
    #endif

        using P = test_parser<true>;

        parsegrind<P>(cn("close\r\n"),                         expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn(",close\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn(" close\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("\tclose\r\n"),                       expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close,\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close\t\r\n"),                       expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close\r\n"),                         expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn(" ,\t,,close,, ,\t,,\r\n"),           expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("\r\n close\r\n"),                    expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close\r\n \r\n"),                    expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("any,close\r\n"),                     expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close,any\r\n"),                     expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("any\r\n ,close\r\n"),                expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close\r\n ,any\r\n"),                expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(cn("close,close\r\n"),                   expect_flags{*this, parse_flag::connection_close}); // weird but allowed

        parsegrind<P>(cn("keep-alive\r\n"),                    expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("keep-alive \r\n"),                   expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("keep-alive\t \r\n"),                 expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("keep-alive\t ,x\r\n"),               expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("\r\n keep-alive \t\r\n"),            expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("keep-alive \r\n \t \r\n"),           expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(cn("keep-alive\r\n \r\n"),               expect_flags{*this, parse_flag::connection_keep_alive});

        parsegrind<P>(cn("upgrade\r\n"),                       expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("upgrade \r\n"),                      expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("upgrade\t \r\n"),                    expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("upgrade\t ,x\r\n"),                  expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("\r\n upgrade \t\r\n"),               expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("upgrade \r\n \t \r\n"),              expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(cn("upgrade\r\n \r\n"),                  expect_flags{*this, parse_flag::connection_upgrade});

        // VFALCO What's up with these?
        //parsegrind<P>(cn("close,keep-alive\r\n"),              expect_flags{*this, parse_flag::connection_close | parse_flag::connection_keep_alive});
        parsegrind<P>(cn("upgrade,keep-alive\r\n"),            expect_flags{*this, parse_flag::connection_upgrade | parse_flag::connection_keep_alive});
        parsegrind<P>(cn("upgrade,\r\n keep-alive\r\n"),       expect_flags{*this, parse_flag::connection_upgrade | parse_flag::connection_keep_alive});
        //parsegrind<P>(cn("close,keep-alive,upgrade\r\n"),      expect_flags{*this, parse_flag::connection_close | parse_flag::connection_keep_alive | parse_flag::connection_upgrade});

        parsegrind<P>("GET / HTTP/1.1\r\n\r\n",                expect_keepalive(*this, true));
        parsegrind<P>("GET / HTTP/1.0\r\n\r\n",                expect_keepalive(*this, false));
        parsegrind<P>("GET / HTTP/1.0\r\n"
                   "Connection: keep-alive\r\n\r\n",        expect_keepalive(*this, true));
        parsegrind<P>("GET / HTTP/1.1\r\n"
                   "Connection: close\r\n\r\n",             expect_keepalive(*this, false));

        parsegrind<P>(cn("x\r\n"),                             expect_flags{*this, 0});
        parsegrind<P>(cn("x,y\r\n"),                           expect_flags{*this, 0});
        parsegrind<P>(cn("x ,y\r\n"),                          expect_flags{*this, 0});
        parsegrind<P>(cn("x\t,y\r\n"),                         expect_flags{*this, 0});
        parsegrind<P>(cn("keep\r\n"),                          expect_flags{*this, 0});
        parsegrind<P>(cn(",keep\r\n"),                         expect_flags{*this, 0});
        parsegrind<P>(cn(" keep\r\n"),                         expect_flags{*this, 0});
        parsegrind<P>(cn("\tnone\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(cn("keep,\r\n"),                         expect_flags{*this, 0});
        parsegrind<P>(cn("keep\t\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(cn("keep\r\n"),                          expect_flags{*this, 0});
        parsegrind<P>(cn(" ,\t,,keep,, ,\t,,\r\n"),            expect_flags{*this, 0});
        parsegrind<P>(cn("\r\n keep\r\n"),                     expect_flags{*this, 0});
        parsegrind<P>(cn("keep\r\n \r\n"),                     expect_flags{*this, 0});
        parsegrind<P>(cn("closet\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(cn(",closet\r\n"),                       expect_flags{*this, 0});
        parsegrind<P>(cn(" closet\r\n"),                       expect_flags{*this, 0});
        parsegrind<P>(cn("\tcloset\r\n"),                      expect_flags{*this, 0});
        parsegrind<P>(cn("closet,\r\n"),                       expect_flags{*this, 0});
        parsegrind<P>(cn("closet\t\r\n"),                      expect_flags{*this, 0});
        parsegrind<P>(cn("closet\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(cn(" ,\t,,closet,, ,\t,,\r\n"),          expect_flags{*this, 0});
        parsegrind<P>(cn("\r\n closet\r\n"),                   expect_flags{*this, 0});
        parsegrind<P>(cn("closet\r\n \r\n"),                   expect_flags{*this, 0});
        parsegrind<P>(cn("clog\r\n"),                          expect_flags{*this, 0});
        parsegrind<P>(cn("key\r\n"),                           expect_flags{*this, 0});
        parsegrind<P>(cn("uptown\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(cn("keeper\r\n \r\n"),                   expect_flags{*this, 0});
        parsegrind<P>(cn("keep-alively\r\n \r\n"),             expect_flags{*this, 0});
        parsegrind<P>(cn("up\r\n \r\n"),                       expect_flags{*this, 0});
        parsegrind<P>(cn("upgrader\r\n \r\n"),                 expect_flags{*this, 0});
        parsegrind<P>(cn("none\r\n"),                          expect_flags{*this, 0});
        parsegrind<P>(cn("\r\n none\r\n"),                     expect_flags{*this, 0});

        parsegrind<P>(m("ConnectioX: close\r\n"),              expect_flags{*this, 0});
        parsegrind<P>(m("Condor: close\r\n"),                  expect_flags{*this, 0});
        parsegrind<P>(m("Connect: close\r\n"),                 expect_flags{*this, 0});
        parsegrind<P>(m("Connections: close\r\n"),             expect_flags{*this, 0});

        parsegrind<P>(m("Proxy-Connection: close\r\n"),        expect_flags{*this, parse_flag::connection_close});
        parsegrind<P>(m("Proxy-Connection: keep-alive\r\n"),   expect_flags{*this, parse_flag::connection_keep_alive});
        parsegrind<P>(m("Proxy-Connection: upgrade\r\n"),      expect_flags{*this, parse_flag::connection_upgrade});
        parsegrind<P>(m("Proxy-ConnectioX: none\r\n"),         expect_flags{*this, 0});
        parsegrind<P>(m("Proxy-Connections: 1\r\n"),           expect_flags{*this, 0});
        parsegrind<P>(m("Proxy-Connotes: see-also\r\n"),       expect_flags{*this, 0});

        failgrind<P>(cn("[\r\n"),                              error::bad_value);
        failgrind<P>(cn("close[\r\n"),                         error::bad_value);
        failgrind<P>(cn("close [\r\n"),                        error::bad_value);
        failgrind<P>(cn("close, upgrade [\r\n"),               error::bad_value);
        failgrind<P>(cn("upgrade[]\r\n"),                      error::bad_value);
        failgrind<P>(cn("keep\r\n -alive\r\n"),                error::bad_value);
        failgrind<P>(cn("keep-alive[\r\n"),                    error::bad_value);
        failgrind<P>(cn("keep-alive []\r\n"),                  error::bad_value);
        failgrind<P>(cn("no[ne]\r\n"),                         error::bad_value);
    }

    void
    testContentLengthField()
    {
        using P = test_parser<true>;
        auto const c = [](std::string const& s)
            { return "GET / HTTP/1.1\r\nContent-Length: " + s + "\r\n"; };
        auto const m = [](std::string const& s)
            { return "GET / HTTP/1.1\r\n" + s + "\r\n"; };
        auto const check =
            [&](std::string const& s, std::uint64_t v)
            {
                parsegrind<P>(c(s),
                    [&](P const& p)
                    {
                        BEAST_EXPECT(p.content_length());
                        BEAST_EXPECT(p.content_length() && *p.content_length() == v);
                    }, true);
            };

        check("0\r\n",          0);
        check("00\r\n",         0);
        check("1\r\n",          1);
        check("01\r\n",         1);
        check("9\r\n",          9);
        check("42 \r\n",        42);
        check("42\t\r\n",       42);
        check("42 \t \r\n",     42);
        check("42\r\n \t \r\n", 42);

        parsegrind<P>(m("Content-LengtX: 0\r\n"),           expect_flags{*this, 0});
        parsegrind<P>(m("Content-Lengths: many\r\n"),       expect_flags{*this, 0});
        parsegrind<P>(m("Content: full\r\n"),               expect_flags{*this, 0});

        failgrind<P>(c("\r\n"),                             error::bad_content_length);
        failgrind<P>(c("18446744073709551616\r\n"),         error::bad_content_length);
        failgrind<P>(c("0 0\r\n"),                          error::bad_content_length);
        failgrind<P>(c("0 1\r\n"),                          error::bad_content_length);
        failgrind<P>(c(",\r\n"),                            error::bad_content_length);
        failgrind<P>(c("0,\r\n"),                           error::bad_content_length);
        failgrind<P>(m(
            "Content-Length: 0\r\nContent-Length: 0\r\n"),  error::bad_content_length);
    }

    void
    testTransferEncodingField()
    {
        auto const m = [](std::string const& s)
            { return "GET / HTTP/1.1\r\n" + s + "\r\n"; };
        auto const ce = [](std::string const& s)
            { return "GET / HTTP/1.1\r\nTransfer-Encoding: " + s + "\r\n0\r\n\r\n"; };
        auto const te = [](std::string const& s)
            { return "GET / HTTP/1.1\r\nTransfer-Encoding: " + s + "\r\n"; };

        using P = test_parser<true>;

        parsegrind<P>(ce("chunked\r\n"),                expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked \r\n"),               expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked\t\r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked \t\r\n"),             expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(" chunked\r\n"),               expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("\tchunked\r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked,\r\n"),               expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked ,\r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked, \r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(",chunked\r\n"),               expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(", chunked\r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(" ,chunked\r\n"),              expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("chunked\r\n \r\n"),           expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("\r\n chunked\r\n"),           expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(",\r\n chunked\r\n"),          expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("\r\n ,chunked\r\n"),          expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce(",\r\n chunked\r\n"),          expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("gzip, chunked\r\n"),          expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("gzip, chunked \r\n"),         expect_flags{*this, parse_flag::chunked});
        parsegrind<P>(ce("gzip, \r\n chunked\r\n"),     expect_flags{*this, parse_flag::chunked});

        // Technically invalid but beyond the parser's scope to detect
        // VFALCO Look into this
        //parsegrind<P>(ce("custom;key=\",chunked\r\n"),  expect_flags{*this, parse_flag::chunked});

        parsegrind<P>(te("gzip\r\n"),                   expect_flags{*this, 0});
        parsegrind<P>(te("chunked, gzip\r\n"),          expect_flags{*this, 0});
        parsegrind<P>(te("chunked\r\n , gzip\r\n"),     expect_flags{*this, 0});
        parsegrind<P>(te("chunked,\r\n gzip\r\n"),      expect_flags{*this, 0});
        parsegrind<P>(te("chunked,\r\n ,gzip\r\n"),     expect_flags{*this, 0});
        parsegrind<P>(te("bigchunked\r\n"),             expect_flags{*this, 0});
        parsegrind<P>(te("chunk\r\n ked\r\n"),          expect_flags{*this, 0});
        parsegrind<P>(te("bar\r\n ley chunked\r\n"),    expect_flags{*this, 0});
        parsegrind<P>(te("barley\r\n chunked\r\n"),     expect_flags{*this, 0});

        parsegrind<P>(m("Transfer-EncodinX: none\r\n"), expect_flags{*this, 0});
        parsegrind<P>(m("Transfer-Encodings: 2\r\n"),   expect_flags{*this, 0});
        parsegrind<P>(m("Transfer-Encoded: false\r\n"), expect_flags{*this, 0});

        failgrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 1\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n",                                     error::bad_transfer_encoding);
    }

    void
    testUpgradeField()
    {
        auto const m = [](std::string const& s)
            { return "GET / HTTP/1.1\r\n" + s + "\r\n"; };

        using P = test_parser<true>;

        parsegrind<P>(m("Upgrade:\r\n"),                       expect_flags{*this, parse_flag::upgrade});
        parsegrind<P>(m("Upgrade: \r\n"),                      expect_flags{*this, parse_flag::upgrade});
        parsegrind<P>(m("Upgrade: yes\r\n"),                   expect_flags{*this, parse_flag::upgrade});

        parsegrind<P>(m("Up: yes\r\n"),                        expect_flags{*this, 0});
        parsegrind<P>(m("UpgradX: none\r\n"),                  expect_flags{*this, 0});
        parsegrind<P>(m("Upgrades: 2\r\n"),                    expect_flags{*this, 0});
        parsegrind<P>(m("Upsample: 4x\r\n"),                   expect_flags{*this, 0});

        parsegrind<P>(
            "GET / HTTP/1.1\r\n"
            "Connection: upgrade\r\n"
            "Upgrade: WebSocket\r\n"
            "\r\n",
            [&](P const& p)
            {
                BEAST_EXPECT(p.is_upgrade());
            });
    }

    void
    testPartial()
    {
        // Make sure the slow-loris defense works and that
        // we don't get duplicate or missing fields on a split.
        parsegrind<test_parser<true>>(
            "GET / HTTP/1.1\r\n"
            "a: 0\r\n"
            "b: 1\r\n"
            "c: 2\r\n"
            "d: 3\r\n"
            "e: 4\r\n"
            "f: 5\r\n"
            "g: 6\r\n"
            "h: 7\r\n"
            "i: 8\r\n"
            "j: 9\r\n"
            "\r\n",
            [&](test_parser<true> const& p)
            {
                BEAST_EXPECT(p.fields.size() == 10);
                BEAST_EXPECT(p.fields.at("a") == "0");
                BEAST_EXPECT(p.fields.at("b") == "1");
                BEAST_EXPECT(p.fields.at("c") == "2");
                BEAST_EXPECT(p.fields.at("d") == "3");
                BEAST_EXPECT(p.fields.at("e") == "4");
                BEAST_EXPECT(p.fields.at("f") == "5");
                BEAST_EXPECT(p.fields.at("g") == "6");
                BEAST_EXPECT(p.fields.at("h") == "7");
                BEAST_EXPECT(p.fields.at("i") == "8");
                BEAST_EXPECT(p.fields.at("j") == "9");
            });
    }

    void
    testLimits()
    {
        {
            multi_buffer b;
            ostream(b) << 
                "POST / HTTP/1.1\r\n"
                "Content-Length: 2\r\n"
                "\r\n"
                "**";
            error_code ec;
            test_parser<true> p;
            p.header_limit(10);
            p.eager(true);
            p.put(b.data(), ec);
            BEAST_EXPECTS(ec == error::header_limit, ec.message());
        }
        {
            multi_buffer b;
            ostream(b) << 
                "POST / HTTP/1.1\r\n"
                "Content-Length: 2\r\n"
                "\r\n"
                "**";
            error_code ec;
            test_parser<true> p;
            p.body_limit(1);
            p.eager(true);
            p.put(b.data(), ec);
            BEAST_EXPECTS(ec == error::body_limit, ec.message());
        }
        {
            multi_buffer b;
            ostream(b) << 
                "HTTP/1.1 200 OK\r\n"
                "\r\n"
                "**";
            error_code ec;
            test_parser<false> p;
            p.body_limit(1);
            p.eager(true);
            p.put(b.data(), ec);
            BEAST_EXPECTS(ec == error::body_limit, ec.message());
        }
        {
            multi_buffer b;
            ostream(b) << 
                "POST / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "2\r\n"
                "**\r\n"
                "0\r\n\r\n";
            error_code ec;
            test_parser<true> p;
            p.body_limit(1);
            p.eager(true);
            p.put(b.data(), ec);
            BEAST_EXPECTS(ec == error::body_limit, ec.message());
        }
    }

    //--------------------------------------------------------------------------

    static
    boost::asio::const_buffers_1
    buf(string_view s)
    {
        return {s.data(), s.size()};
    }

    template<class ConstBufferSequence, bool isRequest, class Derived>
    std::size_t
    feed(ConstBufferSequence const& buffers,
        basic_parser<isRequest, Derived>& p, error_code& ec)
    {
        p.eager(true);
        return p.put(buffers, ec);
    }

    void
    testBody()
    {
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n"
            "\r\n"
            "4\r\nabcd\r\n"
            "0\r\n\r\n"
            ,[&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.body == "abcd");
            });
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Expect: Expires, MD5-Fingerprint\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "*****\r\n"
            "2;a;b=1;c=\"2\"\r\n"
            "--\r\n"
            "0;d;e=3;f=\"4\"\r\n"
            "Expires: never\r\n"
            "MD5-Fingerprint: -\r\n"
            "\r\n"
            ,[&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.body == "*****--");
            });

        parsegrind<test_parser<true>>(
            "GET / HTTP/1.1\r\n"
            "Content-Length: 1\r\n"
            "\r\n"
            "1",
            expect_body(*this, "1"));

        parsegrind<test_parser<false>>(
            "HTTP/1.0 200 OK\r\n"
            "\r\n"
            "hello",
            expect_body(*this, "hello"));

        parsegrind<test_parser<true>>(buffer_cat(
            buf("GET / HTTP/1.1\r\n"
                "Content-Length: 10\r\n"
                "\r\n"),
            buf("12"),
            buf("345"),
            buf("67890")));

        // request without Content-Length or
        // Transfer-Encoding: chunked has no body.
        {
            error_code ec;
            test_parser<true> p;
            feed(buf(
                "GET / HTTP/1.0\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }
        {
            error_code ec;
            test_parser<true> p;
            feed(buf(
                "GET / HTTP/1.1\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // response without Content-Length or
        // Transfer-Encoding: chunked requires eof.
        {
            error_code ec;
            test_parser<false> p;
            feed(buf(
                "HTTP/1.0 200 OK\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.is_done());
            BEAST_EXPECT(p.need_eof());
        }

        // 304 "Not Modified" response does not require eof
        {
            error_code ec;
            test_parser<false> p;
            feed(buf(
                "HTTP/1.0 304 Not Modified\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // Chunked response does not require eof
        {
            error_code ec;
            test_parser<false> p;
            feed(buf(
                "HTTP/1.1 200 OK\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.is_done());
            feed(buf(
                "0\r\n\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // restart: 1.0 assumes Connection: close
        {
            error_code ec;
            test_parser<true> p;
            feed(buf(
                "GET / HTTP/1.0\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // restart: 1.1 assumes Connection: keep-alive
        {
            error_code ec;
            test_parser<true> p;
            feed(buf(
                "GET / HTTP/1.1\r\n"
                "\r\n"
                ), p, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        failgrind<test_parser<true>>(
            "GET / HTTP/1.1\r\n"
            "Content-Length: 1\r\n"
            "\r\n",
            error::partial_message);
    }

    //--------------------------------------------------------------------------

    // https://github.com/vinniefalco/Beast/issues/430
    void
    testIssue430()
    {
        parsegrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n"
            "\r\n"
            "4\r\nabcd\r\n"
            "0\r\n\r\n");
    }

    // https://github.com/vinniefalco/Beast/issues/452
    void
    testIssue452()
    {
        error_code ec;
        test_parser<true> p;
        p.eager(true);
        string_view s =
            "GET / HTTP/1.1\r\n"
            "\r\n"
            "die!";
        p.put(boost::asio::buffer(
            s.data(), s.size()), ec);
        if(! BEAST_EXPECTS(! ec, ec.message()))
            return;
        BEAST_EXPECT(p.is_done());
    }

    // https://github.com/vinniefalco/Beast/issues/496
    void
    testIssue496()
    {
        // The bug affected hex parsing with leading zeroes
        using P = test_parser<false>;
        parsegrind<P>(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n"
            "\r\n"
            "0004\r\nabcd\r\n"
            "0\r\n\r\n"
            ,[&](P const& p)
            {
                BEAST_EXPECT(p.body == "abcd");
            });
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testFlatten();
        testObsFold();
        testCallbacks();
        testRequestLine();
        testStatusLine();
        testFields();
        testConnectionField();
        testContentLengthField();
        testTransferEncodingField();
        testUpgradeField();
        testPartial();
        testLimits();
        testBody();
        testIssue430();
        testIssue452();
        testIssue496();
    }
};

BEAST_DEFINE_TESTSUITE(basic_parser,http,beast);

} // http
} // beast
