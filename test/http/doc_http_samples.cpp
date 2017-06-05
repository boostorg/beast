//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <examples/doc_http_samples.hpp>

#include <beast/core/detail/clamp.hpp>
#include <beast/core/detail/read_size_helper.hpp>
#include <beast/test/pipe_stream.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <sstream>

namespace beast {
namespace http {

class doc_http_samples_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    // two threads, for some examples using a pipe
    doc_http_samples_test()
        : enable_yield_to(2)
    {
    }

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

    void
    doExpect100Continue()
    {
        test::pipe p{ios_};
        yield_to(
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                receive_expect_100_continue(
                    p.server, buffer, ec);
                BEAST_EXPECTS(! ec, ec.message());
            },
            [&](yield_context)
            {
                flat_buffer buffer;
                request<string_body, fields> req;
                req.version = 11;
                req.method("POST");
                req.target("/");
                req.fields.insert("User-Agent", "test");
                req.body = "Hello, world!";
                prepare(req);

                error_code ec;
                send_expect_100_continue(
                    p.client, buffer, req, ec);
                BEAST_EXPECTS(! ec, ec.message());
            });
    }

    void
    doCgiResponse()
    {
        std::string const s = "Hello, world!";
        test::pipe child{ios_};
        child.server.read_size(3);
        ostream(child.server.buffer) << s;
        child.client.close();
        test::pipe p{ios_};
        error_code ec;
        send_cgi_response(child.server, p.client, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<false>(p.server.str(), s));
    }

    void
    doRelay()
    {
        request<string_body, fields> req;
        req.version = 11;
        req.method("POST");
        req.target("/");
        req.fields.insert("User-Agent", "test");
        req.body = "Hello, world!";
        prepare(req);

        test::pipe downstream{ios_};
        downstream.server.read_size(3);
        test::pipe upstream{ios_};
        upstream.client.write_size(3);

        error_code ec;
        write(downstream.client, req);
        BEAST_EXPECTS(! ec, ec.message());
        downstream.client.close();

        flat_buffer buffer;
        relay<true>(upstream.client, downstream.server, buffer, ec,
            [&](header<true, fields>& h, error_code& ec)
            {
                h.fields.erase("Content-Length");
                h.fields.replace("Transfer-Encoding", "chunked");
            });
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<true>(
            upstream.server.str(), req.body));
    }

    void
    doReadStdStream()
    {
        std::string const s =
            "HTTP/1.0 200 OK\r\n"
            "User-Agent: test\r\n"
            "\r\n"
            "Hello, world!";
        std::istringstream is(s);
        error_code ec;
        flat_buffer buffer;
        response<string_body> res;
        read_istream(is, buffer, res, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(boost::lexical_cast<
            std::string>(res) == s);
    }

    void
    doWriteStdStream()
    {
        std::ostringstream os;
        request<string_body> req;
        req.version = 11;
        req.method(verb::get);
        req.target("/");
        req.fields.insert("User-Agent", "test");
        error_code ec;
        write_ostream(os, req, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(boost::lexical_cast<
            std::string>(req) == os.str());
    }

    void
    doCustomParser()
    {
        {
            string_view s{
                "POST / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Hello, world!"
            };
            error_code ec;
            custom_parser<true> p;
            p.put(boost::asio::buffer(
                s.data(), s.size()), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
        {
            string_view s{
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "d\r\n"
                "Hello, world!"
                "\r\n"
                "0\r\n\r\n"
            };
            error_code ec;
            custom_parser<false> p;
            p.put(boost::asio::buffer(
                s.data(), s.size()), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
    }

    void
    doHEAD()
    {
        test::pipe p{ios_};
        yield_to(
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                do_server_head(p.server, buffer, ec);
                BEAST_EXPECTS(! ec, ec.message());
            },
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                auto res = do_head_request(p.client, buffer, "/", ec);
                BEAST_EXPECTS(! ec, ec.message());
            });
    }

    //--------------------------------------------------------------------------
    //
    // Deferred Body type commitment
    //
    //--------------------------------------------------------------------------

    void
    doDeferredBody()
    {
        test::pipe p{ios_};
        ostream(p.server.buffer) <<
            "POST / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "Hello, world!";

        flat_buffer buffer;
        header_parser<true, fields> parser;
        auto bytes_used =
            read_some(p.server, buffer, parser);
        buffer.consume(bytes_used);

        request_parser<string_body> parser2(
            std::move(parser));

        while(! parser2.is_done())
        {
            bytes_used = read_some(p.server, buffer, parser2);
            buffer.consume(bytes_used);
        }
    }

    //--------------------------------------------------------------------------
#if 0
    // VFALCO This is broken
    /*
        Efficiently relay a message from one stream to another
    */
    template<
        bool isRequest,
        class SyncWriteStream,
        class DynamicBuffer,
        class SyncReadStream>
    void
    relay(
        SyncWriteStream& out,
        DynamicBuffer& b,
        SyncReadStream& in)
    {
        flat_buffer buffer{4096}; // 4K limit
        header_parser<isRequest, fields> parser;
        serializer<isRequest, buffer_body<
            typename flat_buffer::const_buffers_type>,
                fields> ws{parser.get()};
        error_code ec;
        do
        {
      	    auto const state0 = parser.state();
            auto const bytes_used =
                read_some(in, buffer, parser, ec);
            BEAST_EXPECTS(! ec, ec.message());
            switch(state0)
            {
            case parse_state::header:
            {
                BEAST_EXPECT(parser.is_header_done());
                for(;;)
                {
                    ws.write_some(out, ec);
                    if(ec == http::error::need_more)
                    {
                        ec = {};
                        break;
                    }
                    if(ec)
                        BOOST_THROW_EXCEPTION(system_error{ec});
                }
                break;
            }

            case parse_state::chunk_header:
            {
                // inspect parser.chunk_extension() here
                if(parser.is_done())
                    boost::asio::write(out,
                        chunk_encode_final());
                break;
            }

            case parse_state::body:
            case parse_state::body_to_eof:
            case parse_state::chunk_body:
            {
                if(! parser.is_done())
                {
                    auto const body = parser.body();
                    boost::asio::write(out, chunk_encode(
                        false, boost::asio::buffer(
                            body.data(), body.size())));
                }
                break;
            }
          
            case parse_state::complete:
                break;
            }
            buffer.consume(bytes_used);
        }
        while(! parser.is_done());
    }

    void
    testRelay()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 5\r\n"
                "\r\n" // 37 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<true>(os, b, is);
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<false>(os, b, is);
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "5;x;y=1;z=\"-\"\r\n*****\r\n"
                "3\r\n---\r\n"
                "1\r\n+\r\n"
                "0\r\n\r\n",
                2 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<true>(os, b, is);
        }
    }
#endif
    
    //--------------------------------------------------------------------------

    void
    run()
    {
        doExpect100Continue();
        doCgiResponse();
        doRelay();
        doReadStdStream();
        doWriteStdStream();
        doCustomParser();
        doHEAD();

        doDeferredBody();
    }
};

BEAST_DEFINE_TESTSUITE(doc_http_samples,http,beast);

} // http
} // beast
