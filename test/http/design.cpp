//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core/flat_buffer.hpp>
#include <beast/core/prepare_buffers.hpp>
#include <beast/http/chunk_encode.hpp>
#include <beast/http/read.hpp>
#include <beast/http/write.hpp>
#include <beast/http/string_body.hpp>
#include <beast/core/detail/clamp.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace beast {
namespace http {

class design_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    //--------------------------------------------------------------------------
    /*
        Read a message with a direct Reader Body.
    */
    struct direct_body
    {
        using value_type = std::string;

        class reader
        {
            value_type& body_;
            std::size_t len_ = 0;

        public:
            static bool constexpr is_direct = true;

            using mutable_buffers_type =
                boost::asio::mutable_buffers_1;

            template<bool isRequest, class Fields>
            explicit
            reader(message<isRequest, direct_body, Fields>& m)
                : body_(m.body)
            {
            }

            void
            init()
            {
            }

            void
            init(std::uint64_t content_length)
            {
                if(content_length >
                        (std::numeric_limits<std::size_t>::max)())
                    throw std::length_error(
                        "Content-Length max exceeded");
                body_.reserve(static_cast<
                    std::size_t>(content_length));
            }

            mutable_buffers_type
            prepare(std::size_t n)
            {
                body_.resize(len_ + n);
                return {&body_[len_], n};
            }

            void
            commit(std::size_t n)
            {
                if(body_.size() > len_ + n)
                    body_.resize(len_ + n);
                len_ = body_.size();
            }

            void
            finish()
            {
                body_.resize(len_);
            }
        };
    };

    void
    testDirectBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            message<true, direct_body, fields> m;
            flat_buffer b{1024};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*"
            };
            message<false, direct_body, fields> m;
            flat_buffer b{20};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            };
            message<true, direct_body, fields> m;
            flat_buffer b{100};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a message with an indirect Reader Body.
    */
    struct indirect_body
    {
        using value_type = std::string;

        class reader
        {
            value_type& body_;

        public:
            static bool constexpr is_direct = false;

            using mutable_buffers_type =
                boost::asio::null_buffers;

            template<bool isRequest, class Fields>
            explicit
            reader(message<isRequest, indirect_body, Fields>& m)
                : body_(m.body)
            {
            }

            void
            init(error_code& ec)
            {
            }
            
            void
            init(std::uint64_t content_length,
                error_code& ec)
            {
            }
            
            void
            write(boost::string_ref const& s,
                error_code& ec)
            {
                body_.append(s.data(), s.size());
            }

            void
            finish(error_code& ec)
            {
            }
        };
    };

    void
    testIndirectBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            message<true, indirect_body, fields> m;
            flat_buffer b{1024};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*"
            };
            message<false, indirect_body, fields> m;
            flat_buffer b{20};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }


        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            };
            message<true, indirect_body, fields> m;
            flat_buffer b{1024};
            read(is, b, m);
            BEAST_EXPECT(m.body == "*");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a message header and manually read the body.
    */
    void
    testManualBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 5\r\n"
                "\r\n" // 37 byte header
                "*****"
            };
            header_parser<true, fields> p;
            flat_buffer b{38};
            auto const bytes_used =
                read_some(is, b, p);
            b.consume(bytes_used);
            BEAST_EXPECT(p.size() == 5);
            BEAST_EXPECT(b.size() < 5);
            b.commit(boost::asio::read(
                is, b.prepare(5 - b.size())));
            BEAST_EXPECT(b.size() == 5);
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****"
            };
            header_parser<false, fields> p;
            flat_buffer b{20};
            auto const bytes_used =
                read_some(is, b, p);
            b.consume(bytes_used);
            BEAST_EXPECT(p.state() ==
                parse_state::body_to_eof);
            BEAST_EXPECT(b.size() < 5);
            b.commit(boost::asio::read(
                is, b.prepare(5 - b.size())));
            BEAST_EXPECT(b.size() == 5);
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a header, check for Expect: 100-continue,
        then conditionally read the body.
    */
    void
    testExpect100Continue()
    {
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Expect: 100-continue\r\n"
                "Content-Length: 5\r\n"
                "\r\n"
                "*****"
            };

            header_parser<true, fields> p;
            flat_buffer b{128};
            auto const bytes_used =
                read_some(is, b, p);
            b.consume(bytes_used);
            BEAST_EXPECT(p.got_header());
            BEAST_EXPECT(
                p.get().fields["Expect"] ==
                    "100-continue");
            message_parser<
                true, string_body, fields> p1{
                    std::move(p)};
            read(is, b, p1);
            BEAST_EXPECT(
                p1.get().body == "*****");
        }
    }

    //--------------------------------------------------------------------------
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
                BEAST_EXPECT(parser.got_header());
                write(out, parser.get());
                break;
            }

            case parse_state::chunk_header:
            {
                // inspect parser.chunk_extension() here
                if(parser.is_complete())
                    boost::asio::write(out,
                        chunk_encode_final());
                break;
            }

            case parse_state::body:
            case parse_state::body_to_eof:
            case parse_state::chunk_body:
            {
                if(! parser.is_complete())
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
        while(! parser.is_complete());
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

    //--------------------------------------------------------------------------
    /*
        Read the request header, then read the request body content using
        a fixed-size buffer, i.e. read the body in chunks of 4k for instance.
        The end of the body should be indicated somehow and chunk-encoding
        should be decoded by beast.
    */
    template<bool isRequest,
        class SyncReadStream, class BodyCallback>
    void
    doFixedRead(SyncReadStream& stream, BodyCallback const& cb)
    {
        flat_buffer buffer{4096}; // 4K limit
        header_parser<isRequest, fields> parser;
        std::size_t bytes_used;
        bytes_used = read_some(stream, buffer, parser);
        BEAST_EXPECT(parser.got_header());
        buffer.consume(bytes_used);
        do
        {
            bytes_used =
                read_some(stream, buffer, parser);
            if(! parser.body().empty())
                cb(parser.body());
            buffer.consume(bytes_used);
        }
        while(! parser.is_complete());
    }
    
    struct bodyHandler
    {
        void
        operator()(boost::string_ref const& body) const
        {
            // called for each piece of the body,
        }
    };

    void
    testFixedRead()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;

        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            doFixedRead<true>(is, bodyHandler{});
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****"
            };
            doFixedRead<false>(is, bodyHandler{});
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
            doFixedRead<true>(is, bodyHandler{});
        }
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testDirectBody();
        testIndirectBody();
        testManualBody();
        testExpect100Continue();
        testRelay();
        testFixedRead();
    }
};

BEAST_DEFINE_TESTSUITE(design,http,beast);

} // http
} // beast
