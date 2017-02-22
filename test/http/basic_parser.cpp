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
#include <beast/core/streambuf.hpp>
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

    template<std::size_t N>
    static
    boost::asio::const_buffers_1
    buf(char const (&s)[N])
    {
        return {s, N-1};
    }

    template<bool isRequest, class Pred>
    void
    good(boost::string_ref const& s,
        Pred const& pred, bool skipBody = false)
    {
        using boost::asio::buffer;
        test_parser<isRequest> p;
        if(skipBody)
            p.set_option(skip_body{true});
        error_code ec;
        auto const n = p.write(buffer(
            s.data(), s.size()), ec);
        if(! BEAST_EXPECTS(! ec, ec.message()))
            return;
        if(! BEAST_EXPECT(n == s.size()))
            return;
        if(p.need_eof())
            p.write_eof(ec);
        if(BEAST_EXPECTS(! ec, ec.message()))
            pred(p);
    }

    template<bool isRequest>
    void
    good(boost::string_ref const& s)
    {
        good<isRequest>(s,
            [](test_parser<isRequest> const&)
            {
            });
    }

    template<bool isRequest>
    void
    bad(boost::string_ref const& s,
        error_code const& ev, bool skipBody = false)
    {
        using boost::asio::buffer;
        test_parser<isRequest> p;
        if(skipBody)
            p.set_option(skip_body{true});
        error_code ec;
        p.write(buffer(
            s.data(), s.size()), ec);
        if(! ec && ev)
            p.write_eof(ec);
        BEAST_EXPECTS(ec == ev, ec.message());
    }

    void
    testFlatten()
    {
        using boost::asio::buffer;
        {
            std::string const s =
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*";
            for(std::size_t i = 1;
                i < s.size() - 1; ++i)
            {
                auto const b1 =
                    buffer(s.data(), i);
                auto const b2 = buffer(
                    s.data() + i, s.size() - i);
                test_parser<true> p;
                error_code ec;
                p.write(b1, ec);
                BEAST_EXPECTS(! ec, ec.message());
                BEAST_EXPECT(p.need_more());
                p.write(buffer_cat(b1, b2), ec);
                BEAST_EXPECTS(! ec, ec.message());
                BEAST_EXPECT(p.is_done());
            }
        }
        {
            std::string const s =
                "HTTP/1.1 200 OK\r\n"
                "\r\n";
            for(std::size_t i = 1;
                i < s.size() - 1; ++i)
            {
                auto const b1 =
                    buffer(s.data(), i);
                auto const b2 = buffer(
                    s.data() + i, s.size() - i);
                test_parser<false> p;
                error_code ec;
                p.write(b1, ec);
                BEAST_EXPECTS(! ec, ec.message());
                BEAST_EXPECT(p.need_more());
                ec = {};
                p.write(buffer_cat(b1, b2), ec);
                BEAST_EXPECTS(! ec, ec.message());
                p.write_eof(ec);
            }
        }
    }

    // Check that all callbacks are invoked
    void
    testCallbacks()
    {
        using boost::asio::buffer;
        {
            test_parser<true> p;
            error_code ec;
            std::string const s =
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*";
            p.write(buffer(s), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_more());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(p.got_on_begin_message);
            BEAST_EXPECT(p.got_on_field);
            BEAST_EXPECT(p.got_on_end_header);
            BEAST_EXPECT(p.got_on_begin_body);
            BEAST_EXPECT(! p.got_on_chunk);
            BEAST_EXPECT(p.got_on_body);
            BEAST_EXPECT(p.got_on_end_body);
            BEAST_EXPECT(p.got_on_end_message);
        }
        {
            test_parser<false> p;
            error_code ec;
            std::string const s =
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*";
            p.write(buffer(s), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_more());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(p.got_on_begin_message);
            BEAST_EXPECT(p.got_on_field);
            BEAST_EXPECT(p.got_on_end_header);
            BEAST_EXPECT(p.got_on_begin_body);
            BEAST_EXPECT(! p.got_on_chunk);
            BEAST_EXPECT(p.got_on_body);
            BEAST_EXPECT(p.got_on_end_body);
            BEAST_EXPECT(p.got_on_end_message);
        }
    }

    void
    testRequestLine()
    {
        good<true>("GET /x HTTP/1.0\r\n\r\n");
        good<true>("!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz / HTTP/1.0\r\n\r\n");
        good<true>("GET / HTTP/1.0\r\n\r\n",            expect_version{*this, 10});
        good<true>("G / HTTP/1.1\r\n\r\n",              expect_version{*this, 11});
        // VFALCO TODO various forms of good request-target (uri)
        good<true>("GET / HTTP/0.1\r\n\r\n",            expect_version{*this, 1});
        good<true>("GET / HTTP/2.3\r\n\r\n",            expect_version{*this, 23});
        good<true>("GET / HTTP/4.5\r\n\r\n",            expect_version{*this, 45});
        good<true>("GET / HTTP/6.7\r\n\r\n",            expect_version{*this, 67});
        good<true>("GET / HTTP/8.9\r\n\r\n",            expect_version{*this, 89});

        bad<true>("\tGET / HTTP/1.0\r\n"    "\r\n",     error::bad_method);
        bad<true>("GET\x01 / HTTP/1.0\r\n"  "\r\n",     error::bad_method);
        bad<true>("GET  / HTTP/1.0\r\n"     "\r\n",     error::bad_path);
        bad<true>("GET \x01 HTTP/1.0\r\n"   "\r\n",     error::bad_path);
        bad<true>("GET /\x01 HTTP/1.0\r\n"  "\r\n",     error::bad_path);
        // VFALCO TODO various forms of bad request-target (uri)
        bad<true>("GET /  HTTP/1.0\r\n"     "\r\n",     error::bad_version);
        bad<true>("GET / _TTP/1.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / H_TP/1.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HT_P/1.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTT_/1.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTTP_1.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/01.2\r\n"     "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/3.45\r\n"     "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/67.89\r\n"    "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/x.0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/1.x\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/1.0 \r\n"     "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/1_0\r\n"      "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/1.0\n\r\n"    "\r\n",     error::bad_version);
        bad<true>("GET / HTTP/1.0\n\r\r\n"  "\r\n",     error::bad_line_ending);
        bad<true>("GET / HTTP/1.0\r\r\n"    "\r\n",     error::bad_line_ending);
    }

    void
    testStatusLine()
    {
        good<false>("HTTP/0.1 200 OK\r\n"   "\r\n",     expect_version{*this, 1});
        good<false>("HTTP/2.3 200 OK\r\n"   "\r\n",     expect_version{*this, 23});
        good<false>("HTTP/4.5 200 OK\r\n"   "\r\n",     expect_version{*this, 45});
        good<false>("HTTP/6.7 200 OK\r\n"   "\r\n",     expect_version{*this, 67});
        good<false>("HTTP/8.9 200 OK\r\n"   "\r\n",     expect_version{*this, 89});
        good<false>("HTTP/1.0 000 OK\r\n"   "\r\n",     expect_status{*this, 0});
        good<false>("HTTP/1.1 012 OK\r\n"   "\r\n",     expect_status{*this, 12});
        good<false>("HTTP/1.0 345 OK\r\n"   "\r\n",     expect_status{*this, 345});
        good<false>("HTTP/1.0 678 OK\r\n"   "\r\n",     expect_status{*this, 678});
        good<false>("HTTP/1.0 999 OK\r\n"   "\r\n",     expect_status{*this, 999});
        good<false>("HTTP/1.0 200 \tX\r\n"  "\r\n",     expect_version{*this, 10});
        good<false>("HTTP/1.1 200  X\r\n"   "\r\n",     expect_version{*this, 11});
        good<false>("HTTP/1.0 200 \r\n"     "\r\n");
        good<false>("HTTP/1.1 200 X \r\n"   "\r\n");
        good<false>("HTTP/1.1 200 X\t\r\n"  "\r\n");
        good<false>("HTTP/1.1 200 \x80\x81...\xfe\xff\r\n\r\n");
        good<false>("HTTP/1.1 200 !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\r\n\r\n");

        bad<false>("\rHTTP/1.0 200 OK\r\n"  "\r\n",     error::bad_line_ending);
        bad<false>("\nHTTP/1.0 200 OK\r\n"  "\r\n",     error::bad_version);
        bad<false>(" HTTP/1.0 200 OK\r\n"   "\r\n",     error::bad_version);
        bad<false>("_TTP/1.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("H_TP/1.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HT_P/1.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTT_/1.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTTP_1.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTTP/01.2 200 OK\r\n"   "\r\n",     error::bad_version);
        bad<false>("HTTP/3.45 200 OK\r\n"   "\r\n",     error::bad_version);
        bad<false>("HTTP/67.89 200 OK\r\n"  "\r\n",     error::bad_version);
        bad<false>("HTTP/x.0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTTP/1.x 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTTP/1_0 200 OK\r\n"    "\r\n",     error::bad_version);
        bad<false>("HTTP/1.0  200 OK\r\n"   "\r\n",     error::bad_status);
        bad<false>("HTTP/1.0 0 OK\r\n"      "\r\n",     error::bad_status);
        bad<false>("HTTP/1.0 12 OK\r\n"     "\r\n",     error::bad_status);
        bad<false>("HTTP/1.0 3456 OK\r\n"   "\r\n",     error::bad_status);
        bad<false>("HTTP/1.0 200\r\n"       "\r\n",     error::bad_status);
        bad<false>("HTTP/1.0 200 \n\r\n"    "\r\n",     error::bad_reason);
        bad<false>("HTTP/1.0 200 \x01\r\n"  "\r\n",     error::bad_reason);
        bad<false>("HTTP/1.0 200 \x7f\r\n"  "\r\n",     error::bad_reason);
        bad<false>("HTTP/1.0 200 OK\n\r\n"  "\r\n",     error::bad_reason);
        bad<false>("HTTP/1.0 200 OK\r\r\n"  "\r\n",     error::bad_line_ending);
    }

    void
    testFields()
    {
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };
        good<true>(m("f:\r\n"));
        good<true>(m("f: \r\n"));
        good<true>(m("f:\t\r\n"));
        good<true>(m("f: \t\r\n"));
        good<true>(m("f: v\r\n"));
        good<true>(m("f:\tv\r\n"));
        good<true>(m("f:\tv \r\n"));
        good<true>(m("f:\tv\t\r\n"));
        good<true>(m("f:\tv\t \r\n"));
        good<true>(m("f:\r\n \r\n"));
        good<true>(m("f:v\r\n"));
        good<true>(m("f: v\r\n u\r\n"));
        good<true>(m("!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz: v\r\n"));
        good<true>(m("f: !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81...\xfe\xff\r\n"));

        bad<true>(m(" f: v\r\n"),                   error::bad_field);
        bad<true>(m("\tf: v\r\n"),                  error::bad_field);
        bad<true>(m("f : v\r\n"),                   error::bad_field);
        bad<true>(m("f\t: v\r\n"),                  error::bad_field);
        bad<true>(m("f: \n\r\n"),                   error::bad_value);
        bad<true>(m("f: v\r \r\n"),                 error::bad_line_ending);
        bad<true>(m("f: \r v\r\n"),                 error::bad_line_ending);
        bad<true>("GET / HTTP/1.1\r\n\r \n\r\n\r\n",error::bad_line_ending);
    }

    void
    testConnectionField()
    {
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };
        auto const cn =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\nConnection: " + s + "\r\n";
            };
        auto const keepalive =
            [&](bool v)
            {
                //return keepalive_f{*this, v};
                return true;
            };

        good<true>(cn("close\r\n"),                         expect_flags{*this, parse_flag::connection_close});
        good<true>(cn(",close\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        good<true>(cn(" close\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("\tclose\r\n"),                       expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close,\r\n"),                        expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close\t\r\n"),                       expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close\r\n"),                         expect_flags{*this, parse_flag::connection_close});
        good<true>(cn(" ,\t,,close,, ,\t,,\r\n"),           expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("\r\n close\r\n"),                    expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close\r\n \r\n"),                    expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("any,close\r\n"),                     expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close,any\r\n"),                     expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("any\r\n ,close\r\n"),                expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close\r\n ,any\r\n"),                expect_flags{*this, parse_flag::connection_close});
        good<true>(cn("close,close\r\n"),                   expect_flags{*this, parse_flag::connection_close}); // weird but allowed

        good<true>(cn("keep-alive\r\n"),                    expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("keep-alive \r\n"),                   expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("keep-alive\t \r\n"),                 expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("keep-alive\t ,x\r\n"),               expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("\r\n keep-alive \t\r\n"),            expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("keep-alive \r\n \t \r\n"),           expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(cn("keep-alive\r\n \r\n"),               expect_flags{*this, parse_flag::connection_keep_alive});

        good<true>(cn("upgrade\r\n"),                       expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("upgrade \r\n"),                      expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("upgrade\t \r\n"),                    expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("upgrade\t ,x\r\n"),                  expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("\r\n upgrade \t\r\n"),               expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("upgrade \r\n \t \r\n"),              expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(cn("upgrade\r\n \r\n"),                  expect_flags{*this, parse_flag::connection_upgrade});

        // VFALCO What's up with these?
        //good<true>(cn("close,keep-alive\r\n"),              expect_flags{*this, parse_flag::connection_close | parse_flag::connection_keep_alive});
        good<true>(cn("upgrade,keep-alive\r\n"),            expect_flags{*this, parse_flag::connection_upgrade | parse_flag::connection_keep_alive});
        good<true>(cn("upgrade,\r\n keep-alive\r\n"),       expect_flags{*this, parse_flag::connection_upgrade | parse_flag::connection_keep_alive});
        //good<true>(cn("close,keep-alive,upgrade\r\n"),      expect_flags{*this, parse_flag::connection_close | parse_flag::connection_keep_alive | parse_flag::connection_upgrade});

        good<true>("GET / HTTP/1.1\r\n\r\n",                expect_keepalive(*this, true));
        good<true>("GET / HTTP/1.0\r\n\r\n",                expect_keepalive(*this, false));
        good<true>("GET / HTTP/1.0\r\n"
                   "Connection: keep-alive\r\n\r\n",        expect_keepalive(*this, true));
        good<true>("GET / HTTP/1.1\r\n"
                   "Connection: close\r\n\r\n",             expect_keepalive(*this, false));

        good<true>(cn("x\r\n"),                             expect_flags{*this, 0});
        good<true>(cn("x,y\r\n"),                           expect_flags{*this, 0});
        good<true>(cn("x ,y\r\n"),                          expect_flags{*this, 0});
        good<true>(cn("x\t,y\r\n"),                         expect_flags{*this, 0});
        good<true>(cn("keep\r\n"),                          expect_flags{*this, 0});
        good<true>(cn(",keep\r\n"),                         expect_flags{*this, 0});
        good<true>(cn(" keep\r\n"),                         expect_flags{*this, 0});
        good<true>(cn("\tnone\r\n"),                        expect_flags{*this, 0});
        good<true>(cn("keep,\r\n"),                         expect_flags{*this, 0});
        good<true>(cn("keep\t\r\n"),                        expect_flags{*this, 0});
        good<true>(cn("keep\r\n"),                          expect_flags{*this, 0});
        good<true>(cn(" ,\t,,keep,, ,\t,,\r\n"),            expect_flags{*this, 0});
        good<true>(cn("\r\n keep\r\n"),                     expect_flags{*this, 0});
        good<true>(cn("keep\r\n \r\n"),                     expect_flags{*this, 0});
        good<true>(cn("closet\r\n"),                        expect_flags{*this, 0});
        good<true>(cn(",closet\r\n"),                       expect_flags{*this, 0});
        good<true>(cn(" closet\r\n"),                       expect_flags{*this, 0});
        good<true>(cn("\tcloset\r\n"),                      expect_flags{*this, 0});
        good<true>(cn("closet,\r\n"),                       expect_flags{*this, 0});
        good<true>(cn("closet\t\r\n"),                      expect_flags{*this, 0});
        good<true>(cn("closet\r\n"),                        expect_flags{*this, 0});
        good<true>(cn(" ,\t,,closet,, ,\t,,\r\n"),          expect_flags{*this, 0});
        good<true>(cn("\r\n closet\r\n"),                   expect_flags{*this, 0});
        good<true>(cn("closet\r\n \r\n"),                   expect_flags{*this, 0});
        good<true>(cn("clog\r\n"),                          expect_flags{*this, 0});
        good<true>(cn("key\r\n"),                           expect_flags{*this, 0});
        good<true>(cn("uptown\r\n"),                        expect_flags{*this, 0});
        good<true>(cn("keeper\r\n \r\n"),                   expect_flags{*this, 0});
        good<true>(cn("keep-alively\r\n \r\n"),             expect_flags{*this, 0});
        good<true>(cn("up\r\n \r\n"),                       expect_flags{*this, 0});
        good<true>(cn("upgrader\r\n \r\n"),                 expect_flags{*this, 0});
        good<true>(cn("none\r\n"),                          expect_flags{*this, 0});
        good<true>(cn("\r\n none\r\n"),                     expect_flags{*this, 0});

        good<true>(m("ConnectioX: close\r\n"),              expect_flags{*this, 0});
        good<true>(m("Condor: close\r\n"),                  expect_flags{*this, 0});
        good<true>(m("Connect: close\r\n"),                 expect_flags{*this, 0});
        good<true>(m("Connections: close\r\n"),             expect_flags{*this, 0});

        good<true>(m("Proxy-Connection: close\r\n"),        expect_flags{*this, parse_flag::connection_close});
        good<true>(m("Proxy-Connection: keep-alive\r\n"),   expect_flags{*this, parse_flag::connection_keep_alive});
        good<true>(m("Proxy-Connection: upgrade\r\n"),      expect_flags{*this, parse_flag::connection_upgrade});
        good<true>(m("Proxy-ConnectioX: none\r\n"),         expect_flags{*this, 0});
        good<true>(m("Proxy-Connections: 1\r\n"),           expect_flags{*this, 0});
        good<true>(m("Proxy-Connotes: see-also\r\n"),       expect_flags{*this, 0});

        bad<true>(cn("[\r\n"),                              error::bad_value);
        bad<true>(cn("close[\r\n"),                         error::bad_value);
        bad<true>(cn("close [\r\n"),                        error::bad_value);
        bad<true>(cn("close, upgrade [\r\n"),               error::bad_value);
        bad<true>(cn("upgrade[]\r\n"),                      error::bad_value);
        bad<true>(cn("keep\r\n -alive\r\n"),                error::bad_value);
        bad<true>(cn("keep-alive[\r\n"),                    error::bad_value);
        bad<true>(cn("keep-alive []\r\n"),                  error::bad_value);
        bad<true>(cn("no[ne]\r\n"),                         error::bad_value);
    }

    void
    testContentLengthField()
    {
        auto const length =
            [&](std::string const& s, std::uint64_t v)
            {
                good<true>(s,
                    [&](test_parser<true> const& p)
                    {
                        BEAST_EXPECT(p.content_length());
                        BEAST_EXPECT(p.content_length() && *p.content_length() == v);
                    }, true);
            };
        auto const c =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\nContent-Length: " + s + "\r\n";
            };
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };

        length(c("0\r\n"),                                  0);
        length(c("00\r\n"),                                 0);
        length(c("1\r\n"),                                  1);
        length(c("01\r\n"),                                 1);
        length(c("9\r\n"),                                  9);
        length(c("123456789\r\n"),                          123456789);
        length(c("42 \r\n"),                                42);
        length(c("42\t\r\n"),                               42);
        length(c("42 \t \r\n"),                             42);

        // VFALCO Investigate this failure
        //length(c("42\r\n \t \r\n"),                         42);

        good<true>(m("Content-LengtX: 0\r\n"),              expect_flags{*this, 0});
        good<true>(m("Content-Lengths: many\r\n"),          expect_flags{*this, 0});
        good<true>(m("Content: full\r\n"),                  expect_flags{*this, 0});

        bad<true>(c("\r\n"),                                error::bad_content_length);
        bad<true>(c("18446744073709551616\r\n"),            error::bad_content_length);
        bad<true>(c("0 0\r\n"),                             error::bad_content_length);
        bad<true>(c("0 1\r\n"),                             error::bad_content_length);
        bad<true>(c(",\r\n"),                               error::bad_content_length);
        bad<true>(c("0,\r\n"),                              error::bad_content_length);
        bad<true>(m(
            "Content-Length: 0\r\nContent-Length: 0\r\n"),  error::bad_content_length);
    }

    void
    testTransferEncodingField()
    {
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };
        auto const ce =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\nTransfer-Encoding: " + s + "\r\n0\r\n\r\n";
            };
        auto const te =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\nTransfer-Encoding: " + s + "\r\n";
            };
        good<true>(ce("chunked\r\n"),                       expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked \r\n"),                      expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked\t\r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked \t\r\n"),                    expect_flags{*this, parse_flag::chunked});
        good<true>(ce(" chunked\r\n"),                      expect_flags{*this, parse_flag::chunked});
        good<true>(ce("\tchunked\r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked,\r\n"),                      expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked ,\r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked, \r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce(",chunked\r\n"),                      expect_flags{*this, parse_flag::chunked});
        good<true>(ce(", chunked\r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce(" ,chunked\r\n"),                     expect_flags{*this, parse_flag::chunked});
        good<true>(ce("chunked\r\n \r\n"),                  expect_flags{*this, parse_flag::chunked});
        good<true>(ce("\r\n chunked\r\n"),                  expect_flags{*this, parse_flag::chunked});
        good<true>(ce(",\r\n chunked\r\n"),                 expect_flags{*this, parse_flag::chunked});
        good<true>(ce("\r\n ,chunked\r\n"),                 expect_flags{*this, parse_flag::chunked});
        good<true>(ce(",\r\n chunked\r\n"),                 expect_flags{*this, parse_flag::chunked});
        good<true>(ce("gzip, chunked\r\n"),                 expect_flags{*this, parse_flag::chunked});
        good<true>(ce("gzip, chunked \r\n"),                expect_flags{*this, parse_flag::chunked});
        good<true>(ce("gzip, \r\n chunked\r\n"),            expect_flags{*this, parse_flag::chunked});

        // Technically invalid but beyond the parser's scope to detect
        // VFALCO Look into this
        //good<true>(ce("custom;key=\",chunked\r\n"),         expect_flags{*this, parse_flag::chunked});

        good<true>(te("gzip\r\n"),                          expect_flags{*this, 0});
        good<true>(te("chunked, gzip\r\n"),                 expect_flags{*this, 0});
        good<true>(te("chunked\r\n , gzip\r\n"),            expect_flags{*this, 0});
        good<true>(te("chunked,\r\n gzip\r\n"),             expect_flags{*this, 0});
        good<true>(te("chunked,\r\n ,gzip\r\n"),            expect_flags{*this, 0});
        good<true>(te("bigchunked\r\n"),                    expect_flags{*this, 0});
        good<true>(te("chunk\r\n ked\r\n"),                 expect_flags{*this, 0});
        good<true>(te("bar\r\n ley chunked\r\n"),           expect_flags{*this, 0});
        good<true>(te("barley\r\n chunked\r\n"),            expect_flags{*this, 0});

        good<true>(m("Transfer-EncodinX: none\r\n"),        expect_flags{*this, 0});
        good<true>(m("Transfer-Encodings: 2\r\n"),          expect_flags{*this, 0});
        good<true>(m("Transfer-Encoded: false\r\n"),        expect_flags{*this, 0});

        bad<false>(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 1\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n",                                         error::bad_transfer_encoding, true);
    }

    void
    testUpgradeField()
    {
        auto const m =
            [](std::string const& s)
            {
                return "GET / HTTP/1.1\r\n" + s + "\r\n";
            };
        good<true>(m("Upgrade:\r\n"),                       expect_flags{*this, parse_flag::upgrade});
        good<true>(m("Upgrade: \r\n"),                      expect_flags{*this, parse_flag::upgrade});
        good<true>(m("Upgrade: yes\r\n"),                   expect_flags{*this, parse_flag::upgrade});

        good<true>(m("Up: yes\r\n"),                        expect_flags{*this, 0});
        good<true>(m("UpgradX: none\r\n"),                  expect_flags{*this, 0});
        good<true>(m("Upgrades: 2\r\n"),                    expect_flags{*this, 0});
        good<true>(m("Upsample: 4x\r\n"),                   expect_flags{*this, 0});

        good<true>(
            "GET / HTTP/1.1\r\n"
            "Connection: upgrade\r\n"
            "Upgrade: WebSocket\r\n"
            "\r\n",
            [&](test_parser<true> const& p)
            {
                BEAST_EXPECT(p.is_upgrade());
            });
    }

    void testBody()
    {
        using boost::asio::buffer;
        good<true>(
            "GET / HTTP/1.1\r\n"
            "Content-Length: 1\r\n"
            "\r\n"
            "1",
            expect_body(*this, "1"));

        good<false>(
            "HTTP/1.0 200 OK\r\n"
            "\r\n"
            "hello",
            expect_body(*this, "hello"));

        // write the body in 3 pieces
        {
            error_code ec;
            test_parser<true> p;
            p.write(buffer_cat(
                buf("GET / HTTP/1.1\r\n"
                    "Content-Length: 10\r\n"
                    "\r\n"),
                buf("12"),
                buf("345"),
                buf("67890")), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(! p.need_eof());
        }

        // request without Content-Length or
        // Transfer-Encoding: chunked has no body.
        {
            error_code ec;
            test_parser<true> p;
            p.write(buf(
                "GET / HTTP/1.0\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(p.is_done());
        }
        {
            error_code ec;
            test_parser<true> p;
            p.write(buf(
                "GET / HTTP/1.1\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(p.is_done());
        }

        // response without Content-Length or
        // Transfer-Encoding: chunked requires eof.
        {
            error_code ec;
            test_parser<false> p;
            p.write(buf(
                "HTTP/1.0 200 OK\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.is_done());
            BEAST_EXPECT(p.need_eof());
            p.write(buf(
                "hello"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.is_done());
            BEAST_EXPECT(p.need_eof());
            p.write_eof(ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // 304 "Not Modified" response does not require eof
        {
            error_code ec;
            test_parser<false> p;
            p.write(buf(
                "HTTP/1.0 304 Not Modified\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(p.is_done());
        }

        // Chunked response does not require eof
        {
            error_code ec;
            test_parser<false> p;
            p.write(buf(
                "HTTP/1.1 200 OK\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(! p.is_done());
            p.write(buf(
                "0\r\n\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(p.is_done());
        }

        // restart: 1.0 assumes Connection: close
        {
            error_code ec;
            test_parser<true> p;
            p.write(buf(
                "GET / HTTP/1.0\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        // restart: 1.1 assumes Connection: keep-alive
        {
            error_code ec;
            test_parser<true> p;
            p.write(buf(
                "GET / HTTP/1.1\r\n"
                "\r\n"
                ), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }

        bad<true>(
            "GET / HTTP/1.1\r\n"
            "Content-Length: 1\r\n"
            "\r\n",
            error::partial_message);
    }

    template<bool isRequest>
    void
    check_header(
        test_parser<isRequest> const& p)
    {
        BEAST_EXPECT(p.got_on_begin_message);
        BEAST_EXPECT(p.got_on_field);
        BEAST_EXPECT(p.got_on_end_header);
        BEAST_EXPECT(! p.got_on_begin_body);
        BEAST_EXPECT(! p.got_on_chunk);
        BEAST_EXPECT(! p.got_on_body);
        BEAST_EXPECT(! p.got_on_end_body);
        BEAST_EXPECT(! p.got_on_end_message);
        BEAST_EXPECT(p.got_header());
    }

    void
    testSplit()
    {
        streambuf sb;
        sb << 
            "POST / HTTP/1.1\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****";
        error_code ec;
        test_parser<true> p;
        p.split(true);
        auto n = p.write(sb.data(), ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(p.got_on_begin_message);
        BEAST_EXPECT(p.got_on_field);
        BEAST_EXPECT(p.got_on_end_header);
        BEAST_EXPECT(! p.got_on_begin_body);
        BEAST_EXPECT(! p.got_on_chunk);
        BEAST_EXPECT(! p.got_on_body);
        BEAST_EXPECT(! p.got_on_end_body);
        BEAST_EXPECT(! p.got_on_end_message);
        BEAST_EXPECT(p.got_header());
        BEAST_EXPECT(! p.is_done());
        BEAST_EXPECT(p.body.empty());
        sb.consume(n);
        p.split(false);
        n = p.write(sb.data(), ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(p.got_on_begin_message);
        BEAST_EXPECT(p.got_on_field);
        BEAST_EXPECT(p.got_on_end_header);
        BEAST_EXPECT(p.got_on_begin_body);
        BEAST_EXPECT(! p.got_on_chunk);
        BEAST_EXPECT(p.got_on_body);
        BEAST_EXPECT(p.got_on_end_body);
        BEAST_EXPECT(p.got_on_end_message);
        BEAST_EXPECT(p.is_done());
        BEAST_EXPECT(p.body == "*****");
    }

    void
    run() override
    {
        testFlatten();
        testCallbacks();
        testRequestLine();
        testStatusLine();
        testFields();
        testConnectionField();
        testContentLengthField();
        testTransferEncodingField();
        testUpgradeField();
        testBody();
        testSplit();
    }
};

BEAST_DEFINE_TESTSUITE(basic_parser,http,beast);

} // http
} // beast
